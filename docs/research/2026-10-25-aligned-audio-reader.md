# Aligned native audio reads — 2026-10-25

## Problem class and evidence

The Death card's measured producer I/O plus DSP time (~2.45s for a2.41s clip,
before scheduler yields) threatens sustained throughput. More prebuffer can hide
short jitter but does not make long playback sustainable. Keep the bounded
worker/provider lifecycle and change only the worker's file-read path.

## Primary-source check

Installed ESP-IDF5.5.4 `components/sdmmc/sdmmc_cmd.c:586–626` checks buffer
alignment and external-RAM location. Classic ESP32 external/unaligned destinations
use a temporary DMA-capable buffer and one-sector reads. Internal aligned memory
can use multi-sector DMA. `components/fatfs/src/ff.c` reads complete sectors
directly when file position is sector aligned; leading/trailing partial sectors
use FatFS's sector window. The official speed guide recommends direct POSIX I/O
or appropriately sized buffers for FatFS:
https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/performance/speed.html

The WAV payload starts at byte78, so merely making an arbitrary payload read's
destination DMA-capable does not establish whole-sector alignment. Retain raw
file boundaries for physical reads and strip the WAV prefix in CPU copies.

## Decision

The sole producer opens unbuffered stdio, validates the same WAV metadata, then uses direct POSIX `read(fileno(file), ...)` for
4096-byte raw-file chunks starting on4096-byte file offsets into a4096-byte
internal DMA-capable buffer. A bounded helper copies only the requested PCM
range into the existing PSRAM blocks. Each20ms block needs at most two cache
fills. The final fill ends at the PCM extent, so authored trailing chunks are
never exposed as samples. Allocate the DMA buffer only while a worker-owned
file is active; release it on EOF, load/read failure and acknowledged cancel.
No allocation, I/O or analysis moves into the Bluetooth callback. Preserve32
blocks and mandatory two-publication yielding.

## Verification boundary

Host production-source tests must exercise non-word-aligned byte78 payloads,
varied sample values, exact PCM output/hash, a truncated read, allocation failure,
EOF/cancellation ownership and buffer cleanup. Hardware throughput, internal
heap margin and audible quality remain parent-owned bench checks; compile/tests
alone do not establish the static is fixed.

## Host result

`python3 native_scripts/run_host_tests.py` passed with AddressSanitizer and
UndefinedBehaviorSanitizer after the change. The actual producer test covers a
byte78 PCM offset with varied stereo samples and a trailing RIFF chunk; output
samples and raw FNV hash match exactly. Read seams assert aligned addresses,
4096-byte physical offsets and no reads beyond the PCM extent. Injected short
reads and internal-DMA allocation failure reach terminal error and release both
file and cache. Existing partial-provider cancellation, held final-close ownership,
role gain/muting, bounded two-block bursts and starvation tests still pass.
Native target compilation and physical throughput/heap/audible qualification are
not performed by this task and remain required before adopting the change.
