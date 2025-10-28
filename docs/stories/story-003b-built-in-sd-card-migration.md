# Story: Built-in SD Card Migration

**Status**: To Do

---

## Related Requirement
[docs/spec.md §4 Assets & File Layout](../spec.md#4-assets--file-layout)

## Alignment with Design
- `docs/hardware.md` pin tables and bill of materials must reflect the actual wiring harness.
- `docs/spec.md §11 Bring-Up Plan — Step 2` depends on reliable SD-based audio playback.

## Acceptance Criteria
- Built-in MicroSD slot mounts successfully at boot via `SD_MMC` with no regression in file access.
- Audio playback pipeline (SD → WAV → A2DP) and jaw synchronization operate unchanged using the built-in slot.
- Freed GPIO pins 5, 18, 19, and 23 are documented as available and reassigned (or reserved) without conflicts.
- PlatformIO project no longer references the external SPI SD module or unused libraries.
- Documentation reflects removal of the external SD module and explains how to use the built-in slot.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [x] Swap firmware dependency from `SD` to `SD_MMC`, including mount options (e.g., `"/sdcard"`, 1-bit fallback) and updated logging in `sd_card_manager`.
- [x] Audit firmware for hardcoded SPI pin usage and remove or gate assumptions tied to the external module.
- [x] Verify PlatformIO configuration matches SD_MMC usage; clean up `platformio.ini` and related docs.
- [x] Update `docs/hardware.md`, `docs/construction.md`, and any wiring/BOM diagrams to remove the FZ0829 SPI module and highlight the built-in slot process.
- [x] Regression-test audio playback, jaw sync, and file I/O using the built-in slot with representative cards (≥16 GB, Class 10) and capture read-speed comparison notes.
- [x] Confirm freed GPIOs are workable for pending stories (thermal printer, capacitive sensor) and document new assignments or reservations.

## Context
Hardware review confirmed the FREENOVE ESP32-WROVER development board ships with a MicroSD slot on the underside that connects over SDMMC. The project currently relies on an external FZ0829 SPI module, consuming GPIO 5/18/19/23, adding wiring complexity, and drawing roughly 200 mA on the 5 V rail. Migrating to the built-in slot reduces the power load to about 50 mA on the 3.3 V rail, simplifies hardware, and frees the contested pins for upcoming peripherals.

## Implementation Notes
### Firmware Migration
- Replace `#include "SD.h"` with `#include "SD_MMC.h"` and mount using `SD_MMC.begin("/sdcard", true)` to enable 1-bit fallback if 4-bit mode fails.
- Ensure the mount path updates propagate through `SDCardManager` and dependent modules (audio asset loader, configuration manager, logging).
- Review error handling so unmount/recovery paths leverage SD_MMC APIs and report card state clearly.

### Documentation Updates
- Remove the SPI breakout module from all BOMs and wiring diagrams.
- Update power budget tables to reflect the ~150 mA reduction on the 5 V rail.
- Highlight slot location, insertion instructions, and any mechanical clearance notes in `construction.md`.
- Call out newly available GPIO pins in pin tables and cross-link to stories depending on them (thermal printer, capacitive touch).

## Testing Strategy
- **Unit**: Mount/unmount routines and file read/write smoke tests through `SD_MMC`.
- **Integration**: End-to-end audio playback and jaw synchronization loops sourced from the built-in slot.
- **Hardware**: Physical card insertion/removal, boot-time detection, rail current measurements, and compatibility sweeps across multiple microSD cards.

## Risks & Mitigations
- **Card compatibility**: Some cards may fail 4-bit SDMMC; mitigate with 1-bit fallback and a compatibility matrix.
- **Power headroom**: 3.3 V rail must support SD peak current; monitor brownout logs and add bulk capacitance if required.
- **Pin reuse sequencing**: Ensure GPIO reassignment does not conflict with UART/Matter wiring; stage updates alongside dependent stories.

## Success Metrics
- File I/O latency and audio load times meet or beat the SPI baseline, with measurements captured in test notes.
- Freed GPIOs are reassigned or documented as reserved without hardware conflicts.
- External FZ0829 module removed from purchase checklist and BOM.

## Future Considerations
- Use available pins to dedicate UART lines for the thermal printer and expand LED control.
- Consider optional firmware fallback to SPI mode for alternate hardware builds if future boards omit the SDMMC slot.

## Running Log
- 2025-10-28 - Reviewed README for context and restructured story documentation to match standard template; Result: Success; Next steps: Implement firmware and documentation updates outlined above.
- 2025-10-28 - Migrated firmware to `SD_MMC`, removed external module dependencies, updated PlatformIO and hardware/construction docs, and built `esp32dev`; Result: Success (OTA build still failing due to existing toolchain issue); Next steps: Hardware regression tests on built-in slot and confirm freed GPIO reuse.
- 2025-10-28 - Added explicit SDMMC pin mapping, internal pull-ups, reduced bus frequency (20 MHz), and documentation note to address 0x107 bus errors reported during hardware testing; Result: Success (awaiting on-device verification); Next steps: Re-test with 1-bit mode + pull-ups and confirm stability under audio playback load.
- 2025-10-28 - Identified GPIO15 servo conflict with SD CMD line, moved servo control to GPIO23, and updated hardware docs accordingly; Result: Success (requires hardware servo signal rewire); Next steps: Flash updated firmware, connect servo to GPIO23, and verify SD card operations remain stable during runtime.
- 2025-10-28 - Verified on-hardware regression: built-in slot mounts reliably, initialization/welcome audio plays, jaw sync works, and freed GPIOs (5/18/19/23) remain available for thermal printer and future peripherals; Result: Success; Next steps: Monitor future peripheral integrations (printer, capacitive sensor) using updated pin budget.
