# Story 015 — 2026 single-WROVER Death rebuild

**Status:** In Review — software verified; hardware and human acceptance pending
**Date:** 2026-10-25

## Current scope update — 2026-10-25

Cam confirmed the touch sensor has been physically removed. Native touch code,
GPIO setup, sampling, calibration CLI/config and sensor tests are removed.
Trigger/show redesign is explicitly deferred: current priority is stable boot,
speaker reconnect, SD-backed audio and actual HomeKit commissioning/control.
Earlier touch acceptance/work-log claims below describe the initial implementation
and are superseded by this hardware correction.

- [x] Remove native touch sensing without changing radio or existing SD assets.
- [x] Verify speaker pairs and audible synthetic tone (Cam confirmed).
- [x] Verify saved-address reconnect after reboot (demonstrated; one episode took~83s).
- [ ] Resolve current SD-backed speech CRC/cutoff and verify complete startup.
- [ ] Commission the new Matter accessory in Apple Home and observe a command.
- [ ] Reboot after commissioning and verify Home control plus speaker readiness.

## Goal
Use the newer TwoSkulls native ESP-IDF firmware as the underlying implementation, then reapply Death’s single-skull fortune behavior. One classic ESP32-WROVER owns Matter, Wi-Fi, Bluetooth A2DP, onboard SD, jaw/lights, finger detection, and the serial thermal printer. Retire the C3 Super Mini and its UART protocol from the new runtime.

## Source and scope
- Base: `/Users/cam/.codex/worktrees/onboard-sd-migration/TwoSkulls`, branch `codex/onboard-sd-migration`, commit `57ff5cc`; source worktree left unchanged.
- ESP-IDF 5.5.4, managed ESP-Matter 1.5.0, existing checked Bluedroid patch, 240 MHz scheduling and PSRAM audio producer.
- Preserve old Arduino application as rollback/reference. New default documented build lives in `firmware/`.
- Existing Death behavior: far welcome, near fortune invitation, jaw hold, stable touch or timeout, snap, fortune preamble/printing, completion, cooldown; serial diagnostics and cancel.
- Two momentary Matter endpoints replace existing far/near commands; Apple Home must commission Death as a new accessory and automations must be rebound. Never clone the C3 fabric.
- Printer rewrite starts with bounded paced text output. Logo printing is withheld pending protocol/bench qualification because 2025 notes report binary-logo garbage output.
- No physical flashing, provisioning, card writes, commits or pushes in this story’s software build phase.

## Acceptance criteria
- [x] Native firmware builds for ESP32-WROVER with one BTDM radio owner and retained Matter BLE.
- [x] No C3, peer skull, Marco/Polo, or UART handshake required by Death runtime.
- [x] Existing SD audio directories and fortune JSON work in the new application.
- [x] Far/near Matter triggers use bounded cross-task commands; busy/cooldown triggers are ignored without replay.
- [x] Fortune flow handles touch, absent touch, missing assets, cancellation, radio loss and printer failures with bounded waits.
- [x] Printer output is paced, bounded, preserves authored newlines and never treats UART delivery as physical print confirmation.
- [x] Portable production-source tests cover the new flow and inherited audio/storage/actuators.
- [x] Reproducible build, source provenance, wiring, config compatibility and first-flash/rollback instructions are recorded.
- [ ] Human verifies commissioning, audio/jaw, touch, actual paper output and repeated complete flows on Death hardware before Done.

## Tasks
- [x] Inspect both projects and locate the current TwoSkulls worktree.
- [x] Capture existing unrelated edits and establish migration scope.
- [x] Copy native base and inherited host tests without modifying TwoSkulls.
- [x] Adapt native entry point, SD config, recursive audio catalog and startup audio for Death.
- [x] Port Death show, fortune generator and capacitive input.
- [x] Rewrite serial printer and verify output scheduling/formatting.
- [x] Run host suites, native build and independent review; repair findings.
- [x] Document build/bench process and report exact remaining acceptance gates.

## Work Log

### 20261025-1436 — Inspected source and created migration story
- **Result:** Success. The TwoSkulls main checkout is legacy Arduino; current native work is in its migration worktree at `57ff5cc`.
- **Notes:** Death began with an unrelated modified `.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md` and untracked `schematics/`; preserve both. Base native implementation and host harness copied into `firmware/` and `native_scripts/`. Delegated the printer and Death flow as independent implementation tasks.
- **Next:** Adapt entry point/config/catalog; integrate delegated modules and build without flashing hardware.

### 20261025-1441 — Ported Death flow, fortune generation and capacitive input
- **Result:** Success at host-source level. Added `death_show`, `death_fortune`, and `death_touch` modules plus `native_scripts/run_death_tests.py` and three production-source suites. Full device qualification remains pending.
- **Notes:** Far/welcome/near/finger prompt/open hold/snap or timeout/preamble/print once/done/cooldown remain distinct. Existing `fortune_told` content is preferred with `/audio/goodbye` fallback for the shipped assets. Printer completion gating means transport completion only. Trigger/near use atomic single-slot gates; cancellation clears stale requests and waits for audio/SD ownership release. Missing clips advance without wedging; speaker loss, prebuffer failure and print timeout cancel without automatic replay. Found and fixed a preamble prepare-failure edge that could otherwise enqueue after cancellation.
- **Validation:** `python3 native_scripts/run_death_tests.py --idf-path /Users/cam/esp/esp-idf-v5.5.4` passes with warnings-as-errors, AddressSanitizer and UndefinedBehaviorSanitizer. Flow scenarios include both touch branches, missing audio, startup/busy/cooldown gates, delayed prebuffer, cancellation while SD cleanup remains owned, rejected print attempted once, and50 paired concurrent trigger/cancel trials. Fortune tests use SDK cJSON and validate repeated tokens, stale-state reset, malformed/empty JSON/template/wordlist inputs and literal newlines. Touch tests include a30-second gap before first polling to prove calibration still gathers a full second of actual samples, two-second settling, stable detection/removal, invalid reads, frozen interaction baseline and explicit recalibration.
- **Research:** `docs/research/2026-10-25-native-touch.md` records Espressif cached-read guidance, asynchronous sampling decision and unresolved physical response tuning. Preserved original0.1% noise floor/5% threshold ceiling. A configurable rolling history replaces synchronous Arduino multisamples and adds measurable history latency; bench tuning remains required.
- **Next:** Complete native firmware build and hardware checks using the actual SD, sensor, calibrated servo, printer and Apple Home accessory.

### 20261025-1450 — Integrated native runtime and reviewed platform boundaries
- **Result:** Partial success. All inherited host suites and the three Death source suites passed with sanitizers. First target build found unsupported `lstat`; subsequent compile found Xtensa/host uint32_t template-deduction differences. Both corrected in production code.
- **Notes:** Independent review also caught root-inclusive Matter endpoint capacity (raised to3), boot scanner stack budget (16KiB main stack), and missing LED-only error initialization. All are addressed. Original radio/audio/WAV/dependency/patch files remain byte-identical, recorded in base-provenance.json. Added native build/first-migration/bench guide and marked Arduino documentation historical.
- **Next:** Finish final target build, rerun changed suites and record image/size/hash evidence. Physical flash and human acceptance remain pending.

### 20261025-1446 — Final software verification and bench handoff
- **Result:** Success. Final `idf.py -C firmware build size` passed for classic ESP32 with all integrated source changes. `run_host_tests.py` passed (8 Python tests plus9 C++ source suites); `run_death_tests.py` passed all4 suites (flow, touch, actual card assets and fortune), with ASan/UBSan. All9 checked-in Death WAVs were accepted by the production catalog/parser. `git diff --check` passed.
- **Notes:** Printer tests additionally cover explicit blank lines/control-byte sanitization, bounded queue, cancellation epochs, dequeue handoff busy state, final wire drain, and a latched transport fault that rejects further writes/jobs until restart. Generated image and hash/size evidence are in `docs/validation/2026-10-25-native-build.json`. Source base equality verified for14 inherited files. Prior manually entered1450 log header was ahead of local clock; that integration work occurred before1443, and this entry uses the actual local time.
- **Limits:** No hardware flash, commissioning, card writes, commit or push performed. Compilation/sanitizers do not establish physical printer health, touch response, jaw travel, Matter commissioning or full-load heap margin. The factory-only base has no OTA. Story remains In Review until human bench acceptance.
- **Next:** Follow firmware/README.md: preserve original4MiB flash, deliberate first layout migration, commission Death as new accessory, bind far/near automations, run physical end-to-end checks.

### 20261025-1447 — Requested hardware flash; checked connected devices
- **Result:** Blocked by absent USB serial device. Both `/dev/cu.*` and pyserial enumerate only Bluetooth-Incoming-Port and debug-console; no WROVER upload port is present.
- **Notes:** User authorized flashing. No reset, erase or write attempted. Native flash arguments and verified image remain ready.
- **Next:** Connect Death WROVER by USB, identify board, capture/verify original flash, perform first-layout migration and inspect boot.

### 20261025-1453 — Flashed Death WROVER and captured first native boot
- **Result:** Upload success; partial boot qualification. User explicitly requested flash and connected USB. Identified ESP32-D0WD-V3 rev3.1,4MiB flash on `/dev/cu.usbserial-10`. Saved complete original flash in ignored `firmware/build/validation/death-first-flash-20261025/original-full-4MiB.bin`, SHA256`c0164dddd8d30a64b27ef6aab154b12bddb9fc5c578ede0a18549562af9a6471`; esptool verify_flash matched the board digest before any write.
- **Migration:** Original partition table at0x8000 was Arduino dual OTA. Initialized only new NVS0x10000/0x10000, wrote build bootloader0x1000, partition table0xC000 and app0x30000. esptool verified all written hashes. Application SHA256`a7edf0adf0a3009e2a24f22c0ce48e5b6d7bc3e3915437ebab3ac64056b775c7`. No full-chip erase. Backup/log permissions restricted; no provisioning secrets committed.
- **Boot:** PSRAM8MiB detected/4MiB mapped, memory test passed; SD mounted/config loaded; calibrated jaw initialization completed; fortune templates loaded; Matter far1/near2 created; printer UART ready; runtime idle with no resets/panics in the bounded capture. Speaker not connected. No fortune or physical printer job triggered.
- **Unresolved:** SDMMC reads returned0x109 during startup PCM/catalog reads; startup audio failed and catalog admitted7 assets. Touch status raw0/calibratedfalse. These observations prevent claiming complete hardware readiness. Evidence summary is `docs/validation/2026-10-25-first-flash.json`; raw boot log remains ignored/private.
- **Next:** Diagnose physical SD/audio and touch conditions, connect speaker and commission new Death accessory, then complete human bench checks. Story remains In Review.

### 20261025-1455 — Checked current readiness, not only historical errors
- **Result:** Partial. Latest capture cataloged all9 WAVs, but startup PCM load again reported SDMMC0x109. Live status had speaker0/stream0; explicit local initialized.wav playback was ignored. Touch raw0/calibratedfalse. Cannot demonstrate working audio/fortune flow now.
- **Notes:** No explicit reset flag used, but captures include fresh power-on boot logs; opening this USB serial connection may reset the board, so do not label these uninterrupted-running observations. No code/flash/card changes or printer jobs performed.

### 20261025-1500 — Speaker first pairing and audible tone confirmed
- **Result:** Success for Bluetooth connection and synthetic audio. Cam confirmed speaker was powered on only; after pressing its pairing button, exact JBL Flip5 discovery/authentication/A2DP/media start succeeded. Logged speaker1/stream1 and increasing callbacks; native remembered-speaker address save succeeded. Cam confirmed “It paired!” and “I heard a beep.”
- **Notes:** Radio source is unchanged from working native TwoSkulls. The layout migration had fresh NVS, so native needed its first discovery/address seed. No firmware change or reflash required. Added research record and first-pairing guidance to firmware README.
- **Limits:** Tone does not prove SD-backed speech; startup PCM read still failed on this captured boot. Touch remains raw0. Saved address is demonstrated, but repeated reconnect/human complete flow remain unverified. No printer job.
- **Next:** Qualify SD-backed speech and touch separately; keep story In Review pending physical end-to-end acceptance.

### 20261025-1513 — Removed physical sensor dependency; prioritizing boot and HomeKit
- **Result:** Touch implementation, sensing settings and CLI/tests removed from native firmware; historical Arduino rollback remains intact. User explicitly deferred all trigger/show redesign. Existing no-sensor timeout route is retained only as a temporary compatibility path.
- **Tests:** Native build and host suites pass, including new actual startup loader PCM/internals-DMA/allocation-failure coverage. No touch symbols remain in the active build.
- **Hardware evidence:** A stable serial relay avoids repeated open-induced resets on this host. Saved-address reconnect demonstrated after reboot, including an~83s collision/retry episode and an~8s first-attempt boot. Current SD startup/speech failure reproduced independently of historical logs; Cam heard the partial speech.
- **Experiments:**4096-byte internal-DMA staging did not fix20MHz CRC;10MHz upper-clock variant also failed0x109. Both uploads were application-only with hash verification, preserving speaker cache and Matter NVS. Current flashed image SHA256`652464a0a1bd20e028a64ab833ef539325ac8b041fcb7743afb4b45bcee4cf72`. No card writes or printer jobs.
- **Next:** Actual iPhone Home commissioning requested using current advertised setup code; keep board running while user adds it. Bounded independent SD driver audit in progress before choosing another experiment.

### 20261025-1521 — Home control proven; SD mode regression isolated; buffering qualification
- **Result:** User added the Matter accessory with two Home “Outlets.” Commissioning complete observed; Cam tapped one and native Near endpoint2 received it. Firmware correctly ignored the old Near trigger while idle. App updates preserved Home commissioning and speaker cache.
- **SD:**20/10MHz and internal-DMA staging alone reproduced0x109. Source audit found old Arduino units mistake selected HS negotiation; explicitHS now logs APPSD/SDSC960MB at40.00MHz. Full startup425700bytes loaded; warm SD replay completed106425frames with no CRC. Cam heard the whole phrase. This restores a concrete legacy mode difference rather than blaming the previously working card.
- **Remaining audio:** Initial boot clipped first word at800ms preroll. Warm streamed clip had40underruns and audible static; measured refill gap110ms. TwoSkulls prior buffer/throughput research reviewed before changing anything. Built/tested32-block640ms PSRAM buffer with bounded two-block yielding unchanged, plus2s boot preroll. Application-only upload underway; next is short/long playback and reboot/Home retention checks.
- **Scope:** Touch fully removed. Trigger/show redesign still deferred. No fortune/printer jobs, card writes, commits or pushes.

### 20261025-1537 — Isolated remaining audio quality and boot RAM issues
- **Result:**32-block buffer delivered complete106425-frame short clip with0underruns, but Cam still heard static; do not label the audible issue fixed. Diagnostic-only gain and raw-PCM FNV hash added to distinguish legacy attenuation/source integrity from starvation. Old library's100-of127 exponential curve is about41.5% amplitude versus native100% linear unity; this is a hypothesis pending listening A/B.
- **Read throughput:** Short clip read+DSP time (~2.45s before scheduler yields) still threatens long playback. Implemented/tested worker-owned4KiB INTERNAL|DMA, sector-aligned direct reads instead of PSRAM stdio bounce. Tests cover byte78 payloads/trailingchunks/exactPCMhash/shortread/allocfailure/cancelcleanup. DMA memory is released at EOF/error/cancel; callback remains bounded and allocation-free.
- **Memory:** A commissioned boot showed internal minimum~1.2KiB while the16KiB boot stack still existed. Moved recursive catalog path scratch to boot-only PSRAM and reduced main stack to6KiB, freeing10KiB during Matter/Bluetooth bring-up. Native build and host sanitizer suites pass. App-only upload underway; needs device confirmation of heap/stack and long audio.
- **Hardware limitation:** Current speaker reconnect attempts ended in page timeouts; asked whether JBL remains powered. Quieter replay cannot be judged until stream returns. No card writes or printer jobs. Home fabric retained.
- **Next:** Boot new image, verify resource improvement/read CRC/hash/throughput; complete40% listening comparison and long clip when speaker available.

### 20261025-1554 — Longer audio qualified for underruns; static/read-repeatability still open
- **Result:** Current aligned-reader image loaded startup, reconnected after Cam pressed the speaker pairing button, and Cam heard complete startup.12.26s welcome replay completed540540frames/613blocks with0underruns and no CRC. Read total3.315s/max46ms, DSP2.159s, publication gap65ms. Sustained read budget improved substantially versus PSRAM-stdio path. Cam reported a touch of static, including quieter40% tests; volume is therefore not established as the cause/fix.
- **Read-integrity evidence:** In one boot, staging and PSRAM hashes were equal1044401d, ruling out that copy for this observation. Two independent sector-aligned internal-DMA probes of the same unchanged initialized.wav both read425700bytes but produced c9c22729 then47fae215; previous boot hashes also differed. No card writes. This remains an unresolved current data-repeatability issue, independently of audible acceptance or buffer counters.
- **Speaker:** Cam confirmed powered on and not connected elsewhere. Cached attempts repeatedly timed out. Pressing pairing button plus explicit retry connected immediately at236s to the saved target; no new address save/authentication was needed. Do not claim unattended reboot reconnect fully reliable based solely on earlier successful boots.
- **Memory:** New6KiB main stack/PSRAM scan scratch yielded boot internal minimum25.7KiB; later minimum13.9KiB with Home active and probes. Active4KiB read buffer is released at EOF. This improves prior~1.2KiB bring-up minimum; long-soak resource qualification remains pending.
- **Next:** Requested Cam power off Death and connect the microSD to this computer for a read-only independent repeat-read/reference comparison. This differentiates native driver behavior from media/data without another blind audio tweak. No card format/writes, fortune/printer jobs, commits or pushes.

### 20261025 — Card-reader reference and final TwoSkulls build audit
- **Card evidence:** Three read-only, uncached Mac reads of `/Volumes/H-DEATH/audio/initialized.wav` were byte-identical to each other and the repository asset. PCM425700 bytes, FNV1a `d6d8b5d5`, peak19538. Report: `docs/validation/2026-10-25-card-reader-initialized.json`. No card writes. Native probe hashes differed from this reference and each other; device read integrity remains open.
- **User challenge:** Re-audited the actual final TwoSkulls release rather than relying on copied source/defaults. Retained image `0cf0961e…` is the later qualified release, superseding the earlier cited `cf4692a7…`. Final evidence covers all11paired skits, exact frame counts,0underruns/watchdogs and Cam clean-dialogue confirmation.
- **Effective configuration:** Actual generated headers/config and compile commands retain the240MHz CPU, all8SBC `-O2` overrides, PSRAM/affinity/tick settings, bounded two-block producer yields and unchanged radio callback. No omitted final buffer/compiler fix found. Main stack/endpoints/serial identity are the only generated config differences.
- **Important qualification:** Current Death is no longer the exact successful file-read path: troubleshooting introduced32blocks instead of8,4096internal-DMA cache instead of8192PSRAM stdio,40MHz instead of20MHz, and longer startup preroll. These adaptations are not qualified by TwoSkulls results. Corrected provenance to identify the modified audio header and qualified base artifact. No further firmware/flash changes during audit.
- **Next controlled experiment:** Compare exact original TwoSkulls buffered read path and20MHz clock against the fixed reference PCM hash, with card returned to powered-off Death. Prior20MHz CRC failures remain relevant; do not claim restoring a baseline alone fixes them. Pause speculative ring/gain adjustments. Full audits in `docs/research/2026-10-25-twoskulls-build-audit.md` and `docs/research/2026-10-25-twoskulls-history-audit.md`. Story remains In Review.

### 20261025 — Controlled return to final TwoSkulls stream path
- **Change:** Restored the actual qualified `skull_audio.cpp`8-block ring and retained8192-bytePSRAM stdio buffer, preserving only gain/hash diagnostics. Restored20MHz SD clock; startup2s preroll/staging diagnostics retained. Audio source diff now contains only diagnostics. No new buffering workaround.
- **Validation:** All host sanitizer suites and Death flow/asset/fortune suites passed; native build passed. App-only image `1d8eb98f51c1767d0959a335ceb09a8376bbe677477ac5bac64068c0c2513953` flashed with esptool verification. Existing Matter fabric/speaker cache retained. First-attempt saved JBL reconnect around9s, startup complete.20MHz boot/read produced no CRC errors in this bounded run.
- **Listening:** Cam confirmed short initialized replay clean. Longer12.26s welcome was better but still some static. Both clips delivered exact frame counts106425/540540 and0underruns. Long clip max read100ms/refillgap103ms, minimum internalheap11.7KiB. Baseline is materially better but not an established complete fix.
- **Integrity:** Raw startup/stream/direct-probe hashes still differ from independent card reference.20MHz direct probe passes returned014be2d9 thenb5cefb6c instead ofd6d8b5d5. Therefore neither enlarged FIFO nor40MHz explains all observations. Read-only bit-level isolation is next.
- **Next diagnostic:** Added idle-only `sd-window`: two simultaneously retained4096-byte internalDMA buffers read fileoffset98304 independently, each compared byte-for-byte against embedded verifiedreference, then firstbuffer rehashed after secondread. Reports mismatchedbytes/bits/firstoffset and retention stability. Host exact-reference test passed; native build passed; upload in progress. No card writes, printer jobs, commits or pushes.

### 20261025 — Located read differences below filesystem; older-stack control
- **Read-mode/clock tests:** Same fixedwindow differed with512-byte CMD17 and4096-byte CMD18 reads at20MHz,4MHz and400kHz.400kHz also produced one explicitCRC0x109; restored20MHz afterward. Slowclock/single-sector are not demonstrated fixes.
- **Raw-sector mapping:** FAT16,32sectors/cluster(16KiB),cluster5626,absolute database552,LBA180544 for filewindow28672. Derived viaFA_READ f_lseek(offset+1), validatedclust/capacity/windowwithincluster and independentFIL.sect==LBA. BypassedFatFs using sdmmc_read_sectors into internalDMA; both8-sector andsingle-sector passes still differed by roughly33–61bytes/34–63bits. Therefore the differing bytes are present below file buffering/PSRAM/A2DP.
- **Diagnostic defect/fix:** Initial rawprobe placedFIL on skull_diag stack; configuredFIL includes4KiB scratch and overflowed/rebooted. MovedFIL to explicitPSRAM RAII storage. Corrected probe completed withoutoverflow; do not classify the original diagnosticpanic as a spontaneous productfailure.
- **Card status:** Temporarily wrapped only mountedcard host.do_transaction during exclusive rawprobe, forwarding unchanged and recording boundedaggregate R1/transportresults; restoredhook immediatelyafterprobe. Reads and firstSDK CMD13 both reported0x900(ready/TRAN),no cardECC/controller/generalerrorbits,0transport/commanderrors despite differingdata. Known SDK R1visibilitygap is not established as this failure'scause. No sharedSDKmodified.
- **Older-stack control:** Built ignored minimal Arduino2.0.17/IDF4.4.7 SD-only app using exactoriginal SD_MMC.begin request20000000 and GPIO14/15/2. NVS init wrapped toreturnESP_OK; noWiFi/Bluetooth/actuatorinit. Backedup first0x30000 nativeflash,hashd0f33db939114e2f1208a5bd80c3f8bcde9826b3cd597cfe7d45f8b476451bec,andverifiedboarddigestbeforetest. Temporary nativepartitiontablemirror at0x8000 allowedlegacySDKsamepartitionlayout. Nativebootloader/table0xC000 retained. Eight425700-bytePCMreadsallreturnedfullsuccessbut8differentwronghashes(ec1aa899,8600b286,97d23931,79adc53b,cf29b964,b56280bb,dcdcd606,365a3655),reference d6d8b5d5. This is an olderread-stack control, not an exact replay of the entire historicalfirmware/load.
- **Restoration:** Restoring verified nativeprefix and preserved nativeapplication611390fef6c731c293f862b8716b9bc37f3a30f80fbbadd522c6680b74e77282 aftercontrol. NoSDcardwrites,printerjobs,commits orpushes. NativeHome/speakerstate includedinrestore. Mustverifyrestoration/bootbeforeending.
- **Next:** Requested sparecard cross-test; originalcard remainsunchanged. Existing evidence narrowsremainingproblem to SD read path/card/board interaction, butdoesnotestablish a physicallydefectivecardorboard. Furtherbuffer/compiler tuningisnotjustified bytheseobservations.

### 20261025 — Native restoration verified; no spare card; independent SPI comparison
- **Restored:** Esptool appdigest matched, complete nativeprefix0..0x30000digest verified againstbackupbeforeboot, savedconfiguredspeakerloaded andfirstattemptreconnect/startupspeech completed. Nativebaseline restored successfully afterolderSDMMCcontrol.
- **No spare:** Cam has no sparecard. Alternative chosen: onboardSPIreadpath asindependentcontroller/softwareCRCcomparison, using documentedhardware.md mapping CLK14/CMD15/D0=2/D3=13 as SPIclock/MOSI/MISO/CS. PrimarySDK/SDAguidance supportsmodecomparison; returningSPI→SD requirescardpowercycle. Noexistingwiringchanges.
- **SPI probe:** Built ignored minimalArduinoSPIapp at1MHz, NVSinitwrapped/radiosactuatorsoff, readonlyfopen/hashonly, SDKsoftwareCRCwarningsenabled. SavedfreshnativeNVS64KiBforuntouchedcomparison. FlashedonlytemporarylegacySDKpartitiontablemirror0x8000 andprobeapplication0x30000;nativeboot/table0xC000/NVSnotmodified. InitialMCUresetdidnotestablishSPImode(CMD0notoken), sorequestedactual10secondpoweroffofallboardfeedswithcardretained. Probeawaitingthatstep; noformat/cardwrites.
- **Required follow-up:** RunSPIhashcomparisonaftercardpowercycle, restoreoriginal0x8000sectorandnativeapp611390fe… withoutrewritingNVS;verifyfreshNVSsavedbytesunchangedbeforestartingnative. IfSPIentered, requestactualpowercycleagainwhenreturningSDMMC. Noendusercodechangesneeded.

### 20261025 — SPI comparison inconclusive; native restored; new-file test approved
- **SPI result:** Cam fullypowercycledboard/card. SPImountstillreceivednoCMD0token, includingcapturedsubsequentinitialization. NoSPIread-qualityresultwasobtained. Localhardwareguide'sgenericDAT3=13mappingdidnotdemonstrateworkingSPIonthisactualslot; manufacturer'sconfirmedtutorialdescribes1-bit14/15/2only. Do not inferthatSPItestingprovedcarddefectiveorunsupported. Stoppedratherthantryingunverifiedpinassignments.
- **Restored:** Verifiedfreshnative64KiBNVSbackupagainstboardafterSPItest(digestmatched), provingHome/speakerbytesunchanged. Restoredoriginal0x8000sectorfromverifiedprefixbackupandnativeapp611390fe…; esptooldigestsverified. NVSnotrewrittenduringthisrestore. Normalnativebaselineisbackonboard; pendingcardtransfer/powerstep.
- **Approved next test:** Cam explicitlyapprovedbackingupcardexistingfilesandaddingonefreshverifiedclipwithoutchangingexistingfiles. Requestedcardbackincomputerreader; awaitingconnection. Preparedlocal `audio/initialized-readtest-20261025.wav`,425778bytes,fileSHAf5b6edd542b78ca14a9f442f7366d515edef75b0344bdb327fe1f29fdd277ba0,PCM FNVd6d8b5d5. NewfilewillallocateadifferentFATchain; thisdoesnotbyitselfidentifyphysicalNANDfault.
- **Tooling:** `scripts/stage_audio_read_test.py` defaultsreadonlyplan; explicit--write backs upuserfilesintoafreshprivatefolder,verifiescopies,createsonlythenewfilenamewithO_EXCL,fsyncs/verifiesnewandoriginalclips. SkipsprotectedmacOSbookkeepingdirs,recordsmanifestprivately. Localtempfixturesmoketestpassedbackup/readback/originalpreservation/overwrite-refusal. Noactualcardwriteshaveoccurredyet. Originalreadonlyphasepreserveduntilnewexplicitauthorization.

### 20261025 — Backed up card and created the authorized test copy
- **Result:** Cam connected H-DEATH to the computer. Backed up47 existing user files into a fresh private ignored directory, verified every copied file, then exclusively created `audio/initialized-readtest-20261025.wav`. Protected macOS bookkeeping directories were excluded from the user-file backup; no existing card file was overwritten, renamed or deleted.
- **Verification:** Rehashed all47 existing user files after creation; every file still matched its backup. New clip425778bytes matched repository SHA256f5b6edd542b78ca14a9f442f7366d515edef75b0344bdb327fe1f29fdd277ba0 on three uncached reads, PCM FNVd6d8b5d5 and peak19538. Public reports: `docs/validation/2026-10-25-card-test-copy.json`, `2026-10-25-new-card-copy-reads.json`. Private backup/manifests remain ignored with restricted permissions.
- **Handoff:** Safely unmounted H-DEATH(disk4s1). Asked Cam to return the card to powered-off Death and power on. Native firmware is restored; next compare original versus new file in the same boot. No additional card writes, commits or pushes authorized/performed beyond the single approved copy.

### 20261025-1707 — Audio-copy proof recorded; remediation deferred for demo
- **Result:** New identical clip read exact PCM FNVd6d8b5d5 on two device replays, peak19538 and0underruns; Cam confirmed clean. Original clip on the same boot still returneda7fa2f7d/peak32675. This supports file-location-dependent read behavior, not a missing stream-buffer fix.
- **Preparation only:** Prepared a local copy of the card's exact audio folder (40files,9realWAVs,22010672bytes) and proposed retaining oldaudio while using freshly allocated files. Several card assets differ from repository files, so any future rewrite must preserve the verified card versions. No folder rebuild was approved/applied.
- **New direction:** Cam called audio good enough for now and prioritized a demo in an hour. Deferred this investigation and opened Story016 for automatic Start/Waiting/Dead Stop Matter flow and printing. No further card writes during demo work.
