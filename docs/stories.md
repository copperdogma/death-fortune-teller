# Project Stories

Death, the Fortune Telling Skeleton

**Note**: This document serves as an index for all story files in `/docs/stories/`, tracking their progress and status.

---

## Story List
| Story ID | Title                                   | Priority | Status | Link                                                   |
|----------|-----------------------------------------|----------|--------|--------------------------------------------------------|
| 001      | PlatformIO Firmware Bring-Up            | High     | Done   | /docs/stories/story-001-platformio-firmware-bring-up.md |
| 001a     | Configuration Management System         | High     | Done  | /docs/stories/story-001a-configuration-management.md   |
| 001b     | Breathing Cycle Implementation          | High     | Done   | /docs/stories/story-001b-breathing-cycle.md            |
| 002      | SD Audio Playback & Jaw Synchronization | High     | Done   | /docs/stories/story-002-sd-audio-playback-jaw-sync.md   |
| 002b     | Skit Category Support & Directory Structure | High     | Done   | /docs/stories/story-002b-skit-category-support.md       |
| 003      | Matter UART Trigger Handling            | High     | Done  | /docs/stories/story-003-matter-uart-trigger-handling.md |
| 003a     | State Machine Implementation            | High     | Done   | /docs/stories/story-003a-state-machine-implementation.md |
| 003b     | Built-in SD Card Migration              | High     | Done  | /docs/stories/story-003b-built-in-sd-card-migration.md |
| 004      | Capacitive Finger Detection & Snap Flow | High     | Done  | /docs/stories/story-004-cap-sense-snap-flow.md          |
| 004a     | LED Control System & Fault Indicators   | High     | Done  | /docs/stories/story-004a-led-control-system.md         |
| 004b     | Non-Blocking Runtime Refactor           | High     | To Do  | /docs/stories/story-004b-non-blocking-runtime.md       |
| 004c     | Adaptive Finger Sensor Threshold        | High     | In Progress | /docs/stories/story-004c-adaptive-finger-threshold.md   |
| 005      | Thermal Printer Fortune Output          | Medium   | Done | /docs/stories/story-005-thermal-printer-fortune.md      |
| 005a     | Serial Console & Diagnostics            | Medium   | To Do  | /docs/stories/story-005a-serial-console-diagnostics.md |
| 006      | UART Protocol with Matter Controller     | High     | Done   | /docs/stories/story-006-uart-protocol-matter-controller.md |
| 007      | Bluetooth A2DP Reliability & Testing    | Medium   | To Do  | /docs/stories/story-007-bluetooth-a2dp-reliability.md   |
| 008      | ESP-IDF Framework Migration              | Maybe    | To Do  | /docs/stories/story-008-esp-idf-migration.md           |
| 009      | Access WROVER SD Card From USB-C         | Medium   | Future | /docs/stories/story-009-usb-sd-card-access.md          |
| 2026-001 | Detailed Fortune Flow Implementation     | Medium   | Future | /docs/stories/story-2026-001-detailed-fortune-flow.md |

## Notes
- Story IDs map to future files in `/docs/stories/`; create each file as work begins.
- Update priorities and statuses as planning evolves and milestones complete.
- Consider splitting stories if scope expands beyond a single deliverable.
- Story 007 is a "maybe" story for potential framework migration from Arduino to ESP-IDF.

## To Do Later
- newline support when printing fortunes.. See printer story; it was started

## Bugs
- death-matter-controller: never sends the RSP_FABRIC_ACK message when it's connected to Apple Home. I KNOW it is because I can open Apple Home and send commands and it works, and the WROVER receives them.
- MAYBE: SD shutdown over time? it boots, reads SD, plays audio, responds to commmands.. if it sits for a while I got:
  I/Audio: Queued audio for welcome skit: /audio/welcome/welcome.wav
  E (288570) diskio_sdmmc: sdmmc_read_blocks failed (257)
  E/AudioPlayer: Failed to open audio file: /audio/welcome/welcome.wav
