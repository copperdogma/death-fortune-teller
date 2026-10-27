# SDMMC first-error observability

Problem class: intermittent hardware-reported data integrity errors with successful reads between failures. More clock, buffer and retry changes would confound the investigation; first distinguish controller/FIFO faults from card-interface signaling faults.

## Primary evidence

- [ESP-IDF 5.5.4 ESP32 HAL](https://github.com/espressif/esp-idf/blob/v5.5.4/components/hal/esp32/include/hal/sdmmc_ll.h): `sdmmc_ll_get_intr_status()` reads masked `MINTSTS`. The default event mask omits FIFO over/underrun (`FRUN`, bit11). Existing `process_data_status` logs are masked events, despite prior descriptions calling them raw. They establish DCRC/EBE; they cannot establish absence of FRUN.
- [Pinned host driver](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_sdmmc/src/sdmmc_host.c): initial DMA preparation fills four descriptors, each supporting up to 4096 bytes. Both the failing 4096-byte internal audit and 512-byte PSRAM bounce transfer fit the initial descriptors. Simple mid-transfer descriptor-refill starvation is therefore a poor fit, although other controller faults remain possible.
- [Pinned transaction driver](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_sdmmc/src/sdmmc_transaction.c): data errors already reset FIFO and stop DMA. The protocol-layer CMD13 result `0x900` means READY_FOR_DATA plus TRAN; it does not support a card stranded in a data-transfer state.
- [Espressif pull-up requirements](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/sd_pullup_requirements.html): CMD and DAT0–3 require external pull-ups, including unused data pins in 1-bit mode. The guidance also says a GPIO2 LED ordinarily does not affect SD operation. Actual socket pull-network values/routing remain unverified. Healthy idle voltage does not measure resistance, edge shape or transient behavior.

## Decision

Prepare an optional pinned-driver diagnostic that copies full registers to fixed RAM at the first data-error interrupt before clearing/reset, then prints in task context. Record transaction metadata and start-state raw flags so sticky flags can be interpreted. Include full RINTSTS, MINTSTS, IDSTS, controller STATUS and byte counters. Keep the installed SDK unchanged and reject unexpected source versions. Do not change masks, transfer settings, CRC checking or recovery.

Compare an existing good short DMA probe with a failing one. Fresh FIFO/DMA error evidence would justify investigating bus servicing/cache-off timing. DCRC/EBE without such evidence would prioritize socket-side waveform and pull-network inspection. Neither outcome alone proves a root cause.

## Local applicability

Installed 5.5.4 source inspected directly; the status getter, mask and descriptor behavior match the pinned upstream sources. Board 3V3 is 3.291 V; GPIO2 and GPIO15 idle at 3.285 V. These corrected owner readings supersede earlier typos. The unchanged native image passed one Welcome DMA hash audit; intermittency remains unresolved. Capture implementation/build/hardware results belong in Story019; no fix acceptance follows from this research.


## Fresh TwoSkulls comparison requested by owner

Compared the qualified native worktree `/Users/cam/.codex/worktrees/onboard-sd-migration/TwoSkulls`, commit `57ff5cc98ece823ba5b1c64cead6428c33d1240a`. The primary TwoSkulls checkout is legacy `main` at e4b48ff and has no native firmware directory; it is not the relevant comparison. Qualified-worktree release records and retained binary hash corroborate the native baseline (2026-10-15-code-qualification.md; SHA256 0cf0961ed9fdaa414b867ca3258a358eff1464f29c5ef5f0a2a5f4a1879644d9).

- Native mount: both use SDMMC_HOST_DEFAULT, 20 MHz, 1-bit slot, internal-pullup flag, explicit GPIO15/CMD and GPIO2/DAT0 pullups, 5 maximum open files, 16 KiB allocation unit and no formatting. Death's constant SDMMC_FREQ_DEFAULT is the same 20000 kHz.
- Playback: both use eight 882-frame blocks in PSRAM, an 8192-byte PSRAM stdio buffer, four-byte-frame fread, a maximum two-block burst followed by a tick yield, and an unpinned priority5 worker with 4096-byte stack. Death adds hash/gain telemetry; it did not omit a different reader buffer.
- Relevant generated settings match for silicon minimum revision0, PSRAM40MHz, MEMW workaround, 4 KiB always-internal threshold, 32 KiB internal reserve, FatFs allocation/caching/timeout and FreeRTOS100Hz. Whole configurations are not identical: Death adds Matter endpoints/identity, printer, flow and diagnostics workload.
- No actuator overlap with fixed SD pins: jaw23, lights32/33. Card configuration does not select actuator/SD GPIOs.

This confirms parity of the normal mount/read mechanism, not every caller or runtime interaction. It does not prove identical electrical behavior, card/socket condition or binary execution. The new optional full-register capture remains justified because both short DMA and stdio tests can fail before audio delivery.

Manufacturer follow-up: [Freenove FNK0060 repository](https://github.com/Freenove/Freenove_ESP32_WROVER_Board) and local hardware dossier provide board pinouts and onboard SD examples. This bounded pass did not locate a verified socket schematic/resistor network. Do not substitute an Espressif WROVER-KIT schematic for this Freenove board or infer DAT3/CS routing from generic ESP32 IOMUX mapping. The hardware wiring question remains open.
