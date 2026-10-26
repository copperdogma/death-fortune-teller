# Current decision guide

The investigation now concerns incorrect bytes returned by storage reads, rather than audio queue starvation. Parent bench results report sparse bit differences in internal DMA buffers, including raw-sector reads that bypass FatFs. Both single-sector and multi-sector reads, and reduced clock speeds, have reproduced the problem. These are parent-reported observations; this research worker did not operate the board.

The most useful remaining controlled comparison is a read-only **SDSPI** probe on the same socket, after the board's own schematic confirms the wiring. This changes the host peripheral and checks transfer CRC in software. It does not change the card's files.

Important details:

- The SDK example's WROVER-KIT mapping is SCLK14, MOSI15, MISO2, CS13. Root must verify this particular board; the example alone is insufficient.
- Use `SDSPI_HOST_DEFAULT()`, `spi_bus_initialize`, `SDSPI_DEVICE_CONFIG_DEFAULT()`, and `esp_vfs_fat_sdspi_mount`. Set `format_if_mount_failed=false`; include only read operations and hash comparisons.
- Native IDF5.5.4 requires enabling SPI CRC through CMD59 and validates received data with software CRC16. Installed Arduino2.0.17 also requests CRC; it tolerates an old card rejecting CMD59 by disabling CRC. Record that fallback if it occurs and still compare every tested byte/hash with the golden reference.
- **Returning from SPI to SD mode requires an actual card power cycle.** An ESP32 reset while the socket stays powered does not suffice. Plan this step before restoring SDMMC firmware.
- Capture the SDK's first R1 response through a scoped transaction callback if card-error visibility is needed. Another CMD13 issued afterward can miss clear-on-read error flags. Native R1 bits21/20/19 indicate card ECC/controller/general errors; the installed SDK does not decode them when it checks ready/state.
- Always-on clock is a bounded isolation experiment, not restoration of a proven old default: IDF4.4.7 and5.5.4 both enable clock gating. Both also use the same classic-ESP32 clock phase settings. The SD specification permits stopping the clock after the required trailing cycles.

Primary references and detailed source checks follow in the chronological research record. No source, SDK, card, wiring, or hardware changes were made by this research worker.

---

# SD read integrity: next discriminating diagnostic — 2026-10-25

## Problem class and scope

This is an end-to-end read-integrity investigation after an SDK migration, separate from audio throughput. Parent reports repeated successful native file reads with different raw PCM FNV hashes, stable uncached computer reads matching the repository, and equal startup staging/PSRAM hashes. This audit was source-only and did not operate hardware, modify the SDK, or write the card.

## Bounded primary-source pass

Inspected installed ESP-IDF5.5.4 `components/sdmmc/sdmmc_cmd.c`, `sdmmc_sd.c`, `sdmmc_common.c`, `components/fatfs/diskio/diskio_sdmmc.c`, and `components/esp_driver_sdmmc/src/sdmmc_transaction.c`. Versioned upstream counterparts:

- [SD command implementation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/sdmmc/sdmmc_cmd.c)
- [SD initialization and CSD decode](https://github.com/espressif/esp-idf/blob/v5.5.4/components/sdmmc/sdmmc_sd.c)
- [FatFs disk I/O adapter](https://github.com/espressif/esp-idf/blob/v5.5.4/components/fatfs/diskio/diskio_sdmmc.c)
- [SDMMC transaction handling](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_sdmmc/src/sdmmc_transaction.c)

The observed SDSC card reports read_bl_len10 (1024 native bytes), while decoded sector_size512 is intentional. CSD decoding normalizes sectors/capacity; initialization sends CMD16 with512 bytes. Read CMD17/CMD18 uses byte addressing for SDSC (`start_block * block_size`) and sector addressing for SDHC. No missing SDSC addressing adjustment follows from this metadata.

The adapter propagates a failed `sdmmc_read_sectors` to FatFs. DMA destinations can be internal/aligned; PSRAM/unaligned destinations use one-sector internal bounce reads. Data CRC interrupt status maps to ESP_ERR_INVALID_CRC. Existing stale `err` propagation after `sdmmc_wait_for_idle` is relevant to correct error reporting, but source inspection does not establish that it changes bytes already transferred; patching it alone is not an established cure for current PCM instability.

## Decision

First finish the exact qualified TwoSkulls buffered-reader/20MHz comparison already in progress. If hashes remain wrong or variable, measure **byte differences**, rather than adding retries or more queue capacity:

1. During an idle diagnostic, read one bounded file window into INTERNAL buffer A, read it again into a separately retained INTERNAL buffer B, with identical seek/length and checked return values. Keep the two allocations alive until comparison ends; no reused buffer or later PSRAM read obscures the evidence.
2. Report exact bytes read, each hash, mismatch count, first mismatch file offset/expected and actual bytes, and XOR bit counts. A small embedded golden window or per-sector reference checksums can compare both against the independently verified source; same-two-reads alone does not establish correctness.
3. Optionally hash A again after B is read to detect memory overwrite. If initial A changes without another read into A, investigate destination ownership/memory damage before changing bus settings.
4. Keep card writes disabled. Do not mask failures with retries. Only then choose raw-sector versus file-layer isolation based on whether differences are sparse, shifted, sector-sized, or memory-time-dependent.

A convenient non-header window in the verified repository file is **file offset98304,4096 bytes** (aligned to8sectors), CRC32`2df2fb00`, FNV1a`8fe30f82`, SHA256`e371d3fded50f481219303ad3d7455618bd12b11386b558e111bf6beb2d67888`. First32 bytes: `a2e783ec83ecc9e9c9e9fbe2fbe27ce17ce1fbe7fbe748ea48eab6e2b6e2a1de`. It is inside PCM, not the78-byte WAV prefix.

## Local applicability check and uncertainty

Computed the window reference read-only from `sd-card-files/audio/initialized.wav`; a local diagnostic self-check flipped bit2 at byte171 and recovered exactly one mismatch/one changed bit and a different CRC. No device comparison was performed by this audit. Sparse differences suggest different follow-ups from offset/sector duplication, but neither category alone proves a physical card or firmware cause. The retained TwoSkulls configuration audit and computer read evidence remain prerequisites for interpreting the next result.

## Follow-up after baseline/window bench evidence

Parent reports restored20MHz/8-block baseline full-probe mismatches in6–11of104windows. Repeated logged offsets28672,32768,229376,290816,323584 are each the first or last4096-byte window around a32768-byte boundary. This is a clue, not proof of FAT cluster size: actual BPB/csize has not been measured. FatFs `f_read` clips direct multi-sector reads at its actual cluster boundary and fetches the FAT chain there (`ff.c` around3980–4010). Compare direct-seek versus sequential reads of a bad window before assigning a filesystem-boundary cause.

The direct-seek28672 golden-window test then provided more decisive evidence: one read had54mismatched bytes/55bits, firstoffset28685 expected00/actual08; a second read was exact. Firstbuffer stayed unchanged aftersecondread. That excludes the observed PSRAMcopy and lateroverwrite of that retainedbuffer for that trial; it does not locate corruption within card, SD interface, hostDMA, or initialCPUread. The passing98304window was not enough to establish wholefile integrity.

### Next bounded comparison: single versus multiple block commands

Compare the same4096-byte badwindow using eight separate512-byte reads versus one4096-byte read, preserving separateinternalbuffers and exactreference comparison. The installed SDK uses CMD17 for block_count1 and CMD18 for largerreads. An internalaligned4096-byte buffer takesCMD18, whereas PSRAM-backedstdio data is copied through one-sectorbouncebuffers usingCMD17. The rawprobe therefore is **not** identical to the restored TwoSkulls streamingreadpath; collect streamed rawFNV separately before concluding bothfail identically.

On classicESP32 the DMA descriptor maximum4096bytes and fourdescriptorring mean one4096 transfer does not require descriptorrefills. This weakens direct applicability of reports whose trigger is long transfers and ISRdescriptorrefill starvation. Sparsebitdifferences with nocorrespondingreportedCRC are not sufficient to declare wiring/cardfailure: hardwareCRC ordinarily detectslineerrors, and this test has notyet observedthecontrollerrawstatus. If single-block comparison doesnotdiscriminate, collect actualSDMMCdata/statuserrors ratherthanpatching the knownpost-transferidle-status propagationbug speculatively.

Checked [official ESP32 errata](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32/03-errata-description/index.html): olderexternalSRAMwrite/read anddualcoreperipheralreaderrata are fixedbyrev3.0, whilethisboardreportsrev3.1. The remainingCPUperipheralFIFOlimitations do notdirectlydescribeDMAwritteninternalbuffers. No matchingrev3.1SDMMCsilentbit-corruptionerratum wasfound in thisboundedpass.

## Device observations after baseline restoration

At20MHz, original8-block/8192PSRAM stdio path produced complete short and long clips with0underruns; Cam judged short clean and long better but stillstatic. Direct internalDMA full-file probes found6–11bad4096-bytechunks of104. Offsets include28672,32768,229376,290816,323584; many cluster around32KiB boundaries, but actual FAT cluster size is not yet measured. Fixedwindow98304 matched golden twice in two separate diagnostic runs. Retargetedwindow28672 then showed firstpass54mismatchedbytes/55bits while secondpass exactly matched. Repeatinglater produced54–62mismatchedbytes, roughlyonebit/byte; firstmismatch repeatedlyfileoffset28685 expected00 actual08. BufferA rehash afterB wasunchanged, so no observed later overwrite. These are actualbytecomparisons, not merely aggregatehashdifferences.

Keep the distinction between rawprobe(CMD18multi-sector) and restoredPSRAMstdio(bounceCMD17single-sector). Next image supports `sd-window-single` and `sd-single`512-byte reads, plus idle-only reversible `sd-clock`400/4000/10000/20000kHz. Clock changes are diagnostic only and reset to20MHz onboot; no on-card changes. Established SDK `sdmmc_host_set_card_clk` controls divider directly. Compare modes at20MHz first, then lowerclock ifneeded. No siliconerratum or driverpatch has yet been established as the cause.

## Mapping one fixed file window to raw read-only sectors

Further parent evidence: both512-byte and4096-byte reads corrupted the28672 window, and4MHz/400kHz tests also corrupted it. A bounded next isolation is read-only physical-sector transfer after obtaining the file mapping from FatFs. Do not infer consecutive allocation across clusters.

Installed5.5.4 exposes `FATFS.database`, `FATFS.csize`, `FATFS.pdrv`, `FATFS.n_fatent`, `FIL.obj.fs`, `FIL.obj.sclust`, `FIL.clust`, `FIL.sect`. The field is **database**, not dbase. This data-base LBA already includes the mounted partition offset. FatFs `clst2sect` uses `database + (cluster-2)*csize`.

Use `ff_diskio_get_pdrv_card(mounted_card)` (exported in `diskio_sdmmc.h`, include `ff.h` first for BYTE) to select the existing drive. Installed VFS mount uses a FatFs drive string derived directly from that pdrv, e.g. `0:`; `f_open` should receive `0:/audio/initialized.wav`, not the VFS `/sdcard/...` prefix. Open with **FA_READ only**. No mount/unmount/mkfs/write operation is needed.

Seek to **window_offset+1**, check success and exact `f_tell`. This is important: normal and fast `f_lseek` deliberately retain the cluster containing `ofs-1` at an exact cluster boundary. The one-byte-advanced target makes `FIL.clust` describe the window's first sector and also causes the file-sector cache to load that sector. With offset28672 and512-byte sectors:

```
sector_in_cluster = (window_offset / 512) % fs->csize;
lba = fs->database + uint64_t(fil.clust - 2) * fs->csize + sector_in_cluster;
```

Before reading require: mounted card exists; pdrv valid; `fil.obj.fs->pdrv` matches pdrv; FAT type supported; sector size512; `csize>0`; file extent covers offset+4096; offset divisible512; `2<=fil.clust<fs->n_fatent`; and `8<=fs->csize-sector_in_cluster`. That final bound keeps all8sectors within the **same allocated cluster**, so there is no unproven contiguity assumption. Check computed64-bit LBA+8 against cardcapacity and representable API argument before casting. Also compare `FIL.sect` to the calculated LBA after the nonaligned(+1) seek; the owning source sets that cached-sector field to the same first sector. Log filesystem type/csize/database/volbase/pdrv/startcluster/currentcluster/LBA/fileextent.

Copy the validated mapping, close the read-only FIL, and invoke `sdmmc_read_sectors(mounted_card, internal_dma_buffer, lba,8)` or8single-sector calls with checked results. Existing idle/quiescent diagnostic exclusion must hold throughout; avoid concurrent application SDaccess. This bypasses VFS/FatFs **data transfer**, while still using FatFs metadata to find the sectors. Corrupt mapping metadata remains a limitation; stable repeated mappings and exact good reads of the same rawLBA can constrain it.

No source or hardware actions were performed by this mapping audit. Source: installed `components/fatfs/src/ff.h`, `ff.c:f_lseek/clst2sect`, `diskio/diskio_sdmmc.h`, and `vfs/vfs_fat_sdmmc.c:mount_to_vfs_fat`.

## R1 card-error visibility gap — bounded source finding

NativeSD status handling in the installedSDK has a separate visibilitygap from the already-noted staleerror return. `sdmmc_send_cmd_send_status` returns transport/commanderror and copies rawR1 without decoding cardinternalerrorbits. `sdmmc_wait_for_idle` checks only READY_FOR_DATA and CURRENT_STATE==TRAN via `sdmmc_ready_for_data`. Public `sdmmc_get_status` calls that helper and discards the rawstatus entirely. Thus ESP_OK from thosefunctions is not evidence that everyR1carderrorbitwaszero.

The [SD Association physical-layer specification, cardstatus table](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver5.10.pdf) defines nativeR1bit21 CARD_ECC_FAILED,20 CC_ERROR,19 ERROR. These are cleared when theirstatus is read. The [status semantics in the7.10spec](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf) distinguish clear-on-read entries from command-relative and currentstateentries. Therefore issuing an **additionalCMD13after** `sdmmc_read_sectors` may miss anerror alreadyreported/clearedbytheSDK's ownCMD13.

No complete nativeSDR1carderror mask/API wasfound in thisinstalledheader; do not substitute SPI R2masks, whosebitpositions differ. For a focuseddiagnostic use explicitbits `(1u<<21)|(1u<<20)|(1u<<19)`, retain thefull32-bitR1, and also decode commandCRC23/illegalcommand22. Do not turnallnonzerostatusbits intoerrors; normalready/state bitsareexpected.

A scoped, noSDKpatch observation point is the mountedcard's hostcallback `esp_err_t (*do_transaction)(int,sdmmc_command_t*)`. Preserveit, installa wrapperonlythroughanidle/exclusiveprobe, calltheoriginalfirst, and record opcode17/18/13, transportresult, `cmd->error`, and `cmd->response[0]` in afixedboundedbuffer. `MMC_R1(response)` is exactlyresponse[0]. Restorethecallbackonallpaths, then logoutsideit. This captures thefirstCMD13consumedbytheSDK andthereadR1withoutissuingextraSDcommandsorperforminglogginginsideatransaction. It doesnotproveinternalcardECCfailure untilthebitsareactuallyobserved.

A transferCRC validates the card-to-hostpacket, nottheauthenticity of thedata beforethecardformedthatpacket. Internalcardreadfailure is therefore a hypothesis compatible with CRCvalidtransport, but thecurrentevidence doesnotestablishit. No SDK/source/hardwarechanges wereperformedbythisaudit.

## Clock stopping: old/new comparison

Parent asked whether oldSDSCcardcompatibility might depend on continuousclock. Primarysource comparison found **no changeddefault**: official [ESP-IDF4.4.7 sdmmc_host.c](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/driver/sdmmc_host.c) sets `cclk_low_power` when settingcardclock (line226), and installed5.5.4 `sdmmc_host_set_card_clk` does theequivalent via `sdmmc_ll_enable_card_clock_low_power(...,true)`. Bothversions `sdmmc_host_set_cclk_always_on(true)` clearthatbit. ArduinoSD_MMC.cpp doesnotexplicitlyenablealways-onclock.

The basicclassicESP32timing also matches: din/doutphase4(180degrees),corephase0;20MHzhostdivider8;40MHzdivider4;400kHzhostdivider10+carddivider20. There is no identifiedphase/defaultclockgatingmigrationdifference here.

[SD Association physical-layer specification7.10§4.4](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf) permitsclocklowering/stopping, with8trailingcyclesrequiredafterlastresponseordataendbit. Itdoesnotestablish a generalSDSCrequirescontinuousclockrule. The card's actualconformance andcontrollertrailingclockbehaviorwerenotmeasured.

Thus always-on is areversible **isolationexperiment**, notrestorationof a verifiedolddifference. If tested, call `sdmmc_host_set_cclk_always_on(mounted_card->host.slot,true)` **after** any `set_card_clk`, checkESP_OK, runidenticalgoldenrawreadcomparison, thenrestorefalse. A laterclockfrequencychangeexplicitlyreenablesclockgating. No cardwrites/SDKpatches are required. No sourceorhardwareactionsperformedbythisaudit.

## Controlled results

Rawsector bypass validatedFatFsgeometry(FAT16,16KiBclusters,LBA180544) and still returned sparse differingbytes withbothsingle/multi-sector commands. NativeR1 capture duringreadandfirstSDK CMD13 was0x900,0transport/commanderrors; errorvisibilitygap didnotmanifest. LargeFIL diagnostic stackoverflow wasfixed withPSRAM allocation before acceptingrawresults. Lowerclocks4000/400kHz andsingle-sector reads didnotrestorecorrectness. NoSDKpatchapplied.

An isolated Arduino2.0.17/IDF4.4.7 SD-onlycontrol also produced8differentfull-lengthPCMhashes onthissamecard/board. ItsNVSinitializationwasdisabled andnativeflashprefix/applicationbackedupforrestoration. Radiosandactuatorswerenotinitialized. Itdoesnotreplaytheentirehistoricalapplication,but falsifies the narrowerclaim thatwrongbytesrequirethenew5.5.4SDK/Bluetooth/Matterload. Next informativecross-test is anothercardinthisboard, preservingoriginal. IndependentMacreads oforiginalremainstable andbyteidenticaltoreference; currentevidencecannotyetassignphysicalfault.

## Independent SDSPI diagnostic (conditional on verified board wiring)

Parent is checking actual onboard-socket wiring before selecting this experiment. SDK5.5.4 `examples/storage/sd_card/sdspi` documents ESP-WROVER-KIT defaultsocket signals CLK14, CMD15(MOSI), DAT0=2(MISO), DAT3=13(CS). That reference doesnot establishthisspecificboard's wiring; do not repurposepins untilitsownschematicischecked.

Primarymodeconstraint: [SD Association§7.2.1](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver5.10.pdf) describes CMD0withCSasserted selectingSPI, andonlyapowercyclereturningSPI→SD. [Espressif SPIbussharingguide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_share.html) confirmsthecardstaysSPIuntilpoweredoffandon. AnESP32soft/hardresetwithoutcardpowerlossdoesnotestablishSDmodeagain. PlanarealcardpowercyclewhenrestoringSDMMCfirmware; otherwiseitsnextmountfailurecouldsimplybethemodeleftbythetest.

Native5.5.4read-onlysetup follows theinstalledexample'sbus/mountportion:

- `sdmmc_host_t host=SDSPI_HOST_DEFAULT()`, choosebounded400kHzor1MHz initially via `host.max_freq_khz`.
- `spi_bus_config_t` withverifiedMOSI/MISO/SCLK,quadpins=-1, adequate max_transfer_sz; `spi_bus_initialize(host.slot,&bus_cfg,SDSPI_DEFAULT_DMA)`.
- `sdspi_device_config_t dev=SDSPI_DEVICE_CONFIG_DEFAULT()`, set `dev.host_id=host.slot`, `dev.gpio_cs` toverifiedCS.
- Mountwith `esp_vfs_fat_sdspi_mount`, `format_if_mount_failed=false`; openaudio `rb`, repeatgoldenwindow/fullPCMhashreadchecks. Raw `sdmmc_read_sectors` stillusesselectedSDSPIhost ifdesired.
- Unmountwith `esp_vfs_fat_sdcard_unmount`, then `spi_bus_free`. Neverincludeexample'swrite/rename/unlink/formatdemonstrationsteps.

NativeSDK `sdmmc_init_spi_crc` enablesCMD59CRC and SDSPIsoftwarechecksreceiveddataCRC16 (`esp_driver_sdspi/src/sdspi_host.c` around848–855). This isausefulindependentpathfromSDMMCcontrollerhardwareCRC. InstalledArduino2.0.17 `libraries/SD/src/sd_diskio.cpp` alsorequestsCMD59andvalidatesCRC16. Itallowsanoldcard's0x5CMD59responsebydisablingCRC, while5.5.4requiresCRCenabletosucceed. AnisolatedArduinoread-onlySPIprobeusingthecachedtoolchainisthereforereasonable; recordanyCRCfallbackandrequiregoldenexactbytes/hashregardless. Thisisnotpermissiontoformatorsubstitutetestwrites.

No alternativeSDcardwasavailable. No source, wiring, card, or hardwareactionswereperformedbythisaudit.
