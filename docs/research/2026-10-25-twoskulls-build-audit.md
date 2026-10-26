# TwoSkulls effective build audit — 2026-10-25

## Question and scope

Did the Death native port miss the buffering, scheduling, or compiler changes that made TwoSkulls clean? Read-only comparison of the actual successful TwoSkulls worktree at `/Users/cam/.codex/worktrees/onboard-sd-migration/TwoSkulls/firmware` with Death `firmware`, including `sdkconfig`, `build/config/sdkconfig.h`, `build/compile_commands.json`, both CMake patches, and the audio/radio/storage/startup modules. No firmware or card changes performed by this audit.

## Provenance

The actual retained TwoSkulls `build/anthony_wrover.bin` is 2,102,016 bytes and SHA256 `0cf0961ed9fdaa414b867ca3258a358eff1464f29c5ef5f0a2a5f4a1879644d9`. This is the later successful **catalogue-release**, documented in TwoSkulls `docs/validation/2026-10-15-code-qualification.md`: all 11 paired skits passed with exact source frame counts, zero underruns/watchdogs, and Cam confirmed clean dialogue. Earlier implementation/checkin records cite 2,098,304-byte `cf4692a71f1f29405cf85f77b9c03606e826ea2730444cd1c633aec85cc88a25`; that earlier image is superseded for this audit. Both actual build metadata records use `/Users/cam/esp/esp-idf-v5.5.4` and target `esp32`.

## Findings

No missing effective scheduling/PSRAM/SBC configuration was found. Full key comparison of actual `sdkconfig` files differs only in the main task stack (3584 → 6144, including legacy alias), dynamic endpoint count (2 → 3), and test serial identity. Actual generated `build/config/sdkconfig.h` confirms the same differences (without the legacy alias).

Matching effective values include:

- FreeRTOS tick 100 Hz; CPU 240 MHz.
- Bluetooth controller and Bluedroid pinned core 0; Wi-Fi pinned core 1.
- Bluetooth allocates from SPIRAM first; BLE dynamic environment memory enabled.
- CHIP allocation external; mbedTLS external memory allocation.
- PSRAM 40 MHz, malloc enabled, always-internal threshold 4096 bytes, reserved internal memory 32768 bytes, memory test and bankswitch enabled, bankswitch reserve 8, cache workaround enabled with MEMW strategy.
- Project compiler optimization remains `-Os`.

All eight actual SBC encoder compile command entries in **both** builds have optimization sequence `-Os -O2`, so `-O2` wins: `sbc_analysis.c`, `sbc_dct.c`, `sbc_dct_coeffs.c`, `sbc_enc_bit_alloc_mono.c`, `sbc_enc_bit_alloc_ste.c`, `sbc_enc_coeffs.c`, `sbc_encoder.c`, `sbc_packing.c`. The `cmake/sbc_encoder_optimization.cmake` files are byte-identical and assert exactly eight matches. `cmake/matter_bluedroid_patch.cmake` is also byte-identical. This checks actual generated commands, not just default configuration intent.

`main/skull_radio.cpp`, `main/skull_radio.h`, and `main/skull_wave.cpp` are byte-identical. The A2DP callback routing and its Bluetooth integration therefore retain the successful TwoSkulls implementation.

The exact known-good TwoSkulls streaming choices were:

- 882 stereo PCM frames per block (20 ms at 44.1 kHz), eight PSRAM blocks (160 ms total).
- One retained 8192-byte PSRAM stdio buffer attached with `_IOFBF` before wave inspection/read.
- Sole producer worker created with stack 4096, priority 5, no explicit core pinning.
- At most two consecutive produced blocks before a yield. With the actual 100 Hz tick, `max(1, pdMS_TO_TICKS(2))` means one 10 ms tick.
- Startup PCM fully preloaded into PSRAM, gain applied once outside callback, 800 ms preroll.

Death retains the worker priority, stack, bounded two-block burst/yield, PCM block size, provider logic, and radio callback implementation. The current Death diagnostic experiments **deliberately differ** from that successful source in these places:

- Stream ring expanded eight → 32 blocks (640 ms).
- Stream I/O changed from 8192-byte PSRAM stdio buffering to unbuffered file metadata plus a 4096-byte internal DMA raw-read cache. This is a substantive replacement of the known-good I/O path, not a copied TwoSkulls setting.
- Startup read stages through a 4096-byte internal DMA buffer into PSRAM and logs raw hashes; preroll increased 800 → 2000 ms; filename changed for Death.
- SDMMC request increased from 20000 kHz to `SDMMC_FREQ_HIGHSPEED` (40000 kHz). Config lookup supports Death layout.
- Additional PCM hashes/gain diagnostics are present.

## Decision and remaining uncertainty

Do not claim the current Death implementation is exactly the proven TwoSkulls audio path: it initially inherited it, but subsequent diagnostic experiments replace the stream I/O buffering and change SD clock. Conversely, the effective build evidence does not support a missing SBC optimization, core affinity, PSRAM allocation setting, tick rate, or original bounded refill yield. Those important fixes are present.

The narrower unresolved class is data integrity versus downstream scheduling. Parent investigation reports different raw PCM hashes across device reads while read-only computer reads are stable; this audit did not independently run hardware. Such a change happens before A2DP encoding and cannot be explained merely by the inherited callback buffer size. A controlled return to the exact TwoSkulls 20 MHz buffered read path (preserving diagnostics and comparing a fixed PCM hash) would separate the deliberate recent I/O/clock experiments from the original native path. Clean computer audio or zero underruns alone does not establish device read integrity.
