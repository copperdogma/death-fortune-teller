# Death native base and integration decisions — 2026-10-25

## Source selection

The normal `/Users/cam/Documents/Projects/TwoSkulls` checkout is the old Arduino
application. `git worktree list` identified the newer
`/Users/cam/.codex/worktrees/onboard-sd-migration/TwoSkulls`, commit`57ff5cc`
on`codex/onboard-sd-migration`. Its native firmware and validation notes describe
single-WROVER Matter/A2DP operation and paired-skull bench success. That evidence
belongs to TwoSkulls, not this Death board.

Copied that firmware into Death and retained its exact radio, PCM producer, WAV
parser, secondary implementation, dependency lock, partition table, and checked
SDK/SBC patches. `firmware/base-provenance.json` records SHA256 and equality to
the source worktree. The original worktree was read only. The secondary source is
retained because the unchanged radio dispatcher references it; Death never
initializes its server or enables peer discovery. Original `skull_show.cpp` remains
for inherited host regression tests and is absent from the active CMake sources.

Adaptations: app entry point/identity and two Matter endpoints; Death config;
recursive category catalog; single startup clip; calibrated actuators with explicit
jaw hold; new Death controller, cJSON fortunes, touch and text-only printer.
Original Arduino code/PlatformIO remains a clearly documented fallback.

## Problem class: portable filesystem and bounded boot stack

First native compilation rejected `lstat`, which host tests support. Checked the
installed ESP-IDF5.5.4 owning source:
`components/fatfs/vfs/vfs_fat.c` exposes `vfs_fat_stat` and the VFS contract uses
`stat`; FAT has no symbolic links. Adopted `stat` under ESP_PLATFORM and retained
`lstat` in host tests to avoid following symlink cycles. Catalog recursion is
bounded to five directory frames,32 assets and128-byte logical paths. Native
compilation and the host category/symlink test verify applicability. Boot's main
stack is16KiB to accommodate the bounded scan plus C-library calls; this is a
budget, not measured Death hardware stack headroom. It is released when app_main
returns. Runtime heaps still need the full-load bench.

## Problem class: framework endpoint capacity

TwoSkulls has root plus one switch; Death needs root plus far and near switches.
ESP-Matter1.5.0 owning source
`components/esp_matter/data_model/esp_matter_data_model.cpp` bounds all endpoint
creation by `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT`, including root;
`esp_matter_core.cpp` explicitly reserves one for root. Raised the capacity from2
to3 in reproducible defaults and the generated build config. Actual Home endpoint
presentation/automation binding remains a hardware acceptance gate.

## Problem class: safe actuator calibration while boot storage fails

Original installed ServoESP32 `Servo.h` maps configured min/max pulses over
0–180°, then Death uses its0–80° logical jaw and3° endpoint margin. Preserved that
mapping and reversal, rather than inheriting TwoSkulls'544–2400us defaults.
LED-only initialization permits SD/config error blinking without setting up a
servo timer/channel. Native source host tests verify calibrated endpoints,
reversal, hold precedence and no jaw access during LED-only boot diagnostics.
Mechanical travel remains unmeasured on this rebuild.

## Build/tooling findings

ESP-IDF component manager uses psutil to inspect its CMake parent; sandboxed
macOS sysctl access fails. Used the ordinary build with approved process access,
without monkey-patching psutil or dependency tooling. Dependencies were initially
copied from the source project's matching managed cache into Death's ignored
component directory, then checked by the normal lock/manager.

Xtensa's uint32_t aliases unsigned long, unlike this host's unsigned int; made
min/max/clamp template types explicit after native compilation exposed the mismatch.
Host sanitizer tests and actual target compilation are separate evidence.

## Remaining uncertainty

No Death board was flashed, commissioned or exercised. Physical servo travel,
GPIO4/onboard-slot interaction, capacitance harness noise/latency, real printer
paper/status behavior and Matter/A2DP full-load memory/radio health remain bench
work. The native base has no OTA in its4MiB single-factory layout. Restore backup
for rollback instead of mixing its partition map with Arduino OTA offsets.
