# Story: PlatformIO Firmware Bring-Up

**Status**: Done

---

## Related Requirement
[docs/spec.md §11 Bring-Up Plan — Step 1](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Alignment with Design
[docs/spec.md §9 Build & Tooling](../spec.md#9-build--tooling-cursor--platformio)

## Acceptance Criteria
- [x] PlatformIO builds a minimal firmware target for `esp32dev` using `partitions/fortune_ota.csv` (dual OTA) without errors.
- [x] The firmware flashes to the ESP32 over USB and produces readable serial output at 115200 baud on boot.
- [x] OTA upload succeeds for the same build, and the remote log confirms a clean reboot at 115200 baud equivalent via Telnet.
- [x] Environment setup steps and board settings are documented for future contributors.
- [x] User must sign off on functionality before story can be marked complete.

## Tasks
- [x] Audit existing `platformio.ini` and align with spec-directed settings (board, partitions, library deps).
- [x] Implement or update a stub sketch that compiles and runs on the skull hardware.
- [x] Validate flashing workflow, including monitor commands, on actual hardware (USB).
- [x] Validate OTA upload workflow, including log capture via Telnet, on actual hardware.
- [x] Record setup and troubleshooting notes in project docs.

## Notes
- Use this story to lock the ESP32 Arduino core version once stable.
- Coordinate with future OTA work so the baseline environment supports later targets.
- 2025-10-23: `pio run` succeeds for both `esp32dev` and `esp32dev_ota`, now standardized on `partitions/fortune_ota.csv`.
- 2025-10-23: Serial and OTA validations captured (`ai-work/usb-boot-log-20251023.txt`, `ai-work/ota-startup-log-20251023.txt`); user sign-off received.
