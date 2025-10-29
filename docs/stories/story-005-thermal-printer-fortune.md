# Story: Thermal Printer Fortune Output

**Status**: In Progress

---

## Related Requirement
[docs/spec.md §11 Bring-Up Plan — Step 8](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Alignment with Design
[docs/spec.md §4 Assets & File Layout — Printer](../spec.md#4-assets--file-layout)

## Acceptance Criteria
1. Validate JSON file(s) on SD card in `/fortunes` directory on startup (see `little_kid_fortunes.json` for current format). There may be more files in the future, but for now we just have this one.
2. Generate fortune from template when called, filling it in mad-libs style by selecting random words from the wordlists.
3. Print header image + fortune to printer while skeleton keeps talking to entertain kids during printing process. This should occur without audio underruns or noticeable latency spikes.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [x] **Integrate Components**: Connect FortuneGenerator and ThermalPrinter to main.cpp
- [x] **Implement Snap Action**: Add fortune generation and printing to handleFortuneFlow()
- [x] **Implement Bitmap Logo**: Add bitmap logo printing from PoC implementation
- [x] **Add Configuration**: Load fortune file path from config.txt
- [ ] **Test Integration**: Verify complete fortune flow with snap action
- [x] **Document Requirements**: SD card fortune file requirements and maintenance workflow

## Benchmarking
- With 384w logo it takes 19.5 seconds to print, most of that being logo printing time

## Technical Implementation Details

### Component Integration Requirements

**Fortune Generator Integration:**
- Load fortune templates from JSON file on SD card
- Validate JSON format and required fields on startup
- Generate random fortunes from templates using word lists
- Handle fortune generation failures gracefully
- Support configurable fortune file path via config.txt

**Thermal Printer Integration:**
- Initialize printer with correct baud rate and settings
- Handle printer initialization failures gracefully
- Support concurrent printing and audio playback
- Maintain printer performance during extended use
- Handle printer hardware failures without system crash

**Snap Action Integration:**
- Trigger fortune generation immediately after snap action
- Print fortune while continuing audio playback
- Maintain audio continuity during printing process
- Handle printing failures gracefully
- Complete fortune flow regardless of printing success

### Fortune Generation Requirements

**JSON File Validation:**
- Validate fortune file format on system startup
- Check for required fields: version, templates, wordlists
- Validate all template tokens exist in wordlists
- Handle malformed JSON files gracefully
- Provide meaningful error messages for validation failures

**Fortune Generation Logic:**
- Randomly select template from available templates
- Fill template tokens with random words from wordlists
- Generate unique fortunes for each session
- Handle empty or missing wordlists gracefully
- Support multiple fortune templates per session

**Template Processing:**
- Parse template strings for token replacement
- Replace {{token}} placeholders with random words
- Handle missing tokens gracefully
- Ensure generated fortunes are readable and appropriate
- Support complex template structures

### Thermal Printer Requirements

**Printer Initialization:**
- Initialize printer with correct baud rate and settings
- Handle printer connection failures gracefully
- Support printer reconnection if connection lost
- Maintain printer settings across system restarts
- Log printer initialization status for debugging

**Printing Operations:**
- Print logo before fortune text
- Print fortune text with proper formatting
- Handle paper out and other printer errors
- Support concurrent printing and audio operations
- Maintain print quality during extended use

**Bitmap Logo Support:**
- Load bitmap logo from SD card
- Support 1-bit bitmap format (384px wide)
- Handle missing logo files gracefully
- Fall back to text logo if bitmap unavailable
- Maintain logo print quality and alignment

### Audio Integration Requirements

**Concurrent Operations:**
- Maintain audio playback during printing operations
- Prevent audio underruns during printing
- Support jaw movement during printing
- Handle audio timing during fortune flow
- Preserve audio quality during concurrent operations

**Timing Coordination:**
- Ensure skit duration covers printing time
- Coordinate audio and printing completion
- Handle timing mismatches gracefully
- Support variable printing times
- Maintain system responsiveness during printing

### Error Handling and Recovery

**Printer Failures:**
- Handle printer hardware failures gracefully
- Continue audio flow without printing
- Log printer errors for debugging
- Support printer recovery and reconnection
- Maintain system operation with audio-only mode

**Fortune Generation Failures:**
- Handle JSON parsing errors gracefully
- Fall back to default fortune if generation fails
- Log fortune generation errors for debugging
- Continue system operation with fallback content
- Support manual fortune entry if needed

**File Access Errors:**
- Handle SD card access issues gracefully
- Retry file operations when appropriate
- Log file access errors for debugging
- Continue with available content when possible
- Support offline operation with cached content

### Configuration Requirements

**Configurable Parameters:**
- Fortune file path (configurable via config.txt)
- Logo file path (configurable via config.txt)
- Printer baud rate (configurable via config.txt)
- Print quality settings (configurable via config.txt)
- Audio timing coordination (configurable via config.txt)

**Hardware-Specific Settings:**
- Pin assignments hardcoded in firmware (not configurable)
- Printer settings configurable per hardware build
- Default values for all configurable parameters
- Support for different printer models and settings

### Integration Requirements

**State Machine Integration:**
- Integrate with existing state machine transitions
- Maintain existing timing and trigger behavior
- Support all existing state transitions
- Preserve busy policy and debounce logic

**Hardware Integration:**
- Integrate with existing servo control system
- Integrate with existing audio playback system
- Integrate with existing SD card management
- Maintain existing hardware performance and reliability

**Audio Integration:**
- Maintain audio continuity during printing
- Support concurrent audio and printing operations
- Handle audio timing during fortune flow
- Preserve existing audio quality and synchronization

## Notes
- **2025 MVP**: Single fortune file (`little_kid_fortunes.json`) only
- **Hardware**: Use GPIO4/5 per [hardware.md](hardware.md) - no configuration needed
- **Logo**: Implement bitmap logo from PoC, fallback to text logo if file missing
- **Error Handling**: Ignore printer failures, continue with audio-only
- **Audio Timing**: Ensure skit is long enough to cover printing time (test during integration)
- Monitor printer power draw; record any mitigation (caps, wiring) required for reliable output
- Plan for future mode-specific fortune sets once JSON format stabilizes

## Fortune File Workflow
- Keep production fortune templates as JSON files under `/fortunes/`. The firmware reads the path from `fortunes_json` in `config.txt` and falls back to `/printer/fortunes_littlekid.json` if the key is missing.
- Each file must provide `version`, `templates`, and `wordlists` fields. Token names in `templates` must match keys in `wordlists`; the loader validates this on boot and logs any mismatch.
- To update fortunes: edit the source JSON locally, copy it to the SD card with a unique filename (e.g., `/fortunes/little_kid_fortunes_v2.json`), point `fortunes_json` at the new file, and reboot the skull. Leave the previous file on disk until the new content is validated on hardware.
- Avoid macOS metadata files (e.g., `._foo.json`). The loader ignores hidden entries but large numbers of them slow the directory scan. Run `dot_clean` after copying if needed.
- When testing changes without redeploying assets, use the serial console `print fortune` command to inspect the generated text before triggering the full animatronic flow.

## Printer Asset Requirements
- `printer_logo` in `config.txt` selects the logo; default is `/printer/logo_384w.bmp`. The firmware logs the resolved path on boot and falls back to an ASCII banner if the file is absent.
- Logos must be uncompressed 1-bit BMPs (`BI_RGB`), width ≤384 dots. Narrower artwork is centered automatically; wider files are rejected with a warning.
- Save artwork with a black-on-white palette (index 0 = black, index 1 = white). The renderer inverts the bits to match the printer’s “1 = black” raster command.
- Place logo assets under `/printer/` alongside fortune JSON files so SD copies stay organized. Keep filenames lowercase with no spaces to avoid accidental casing issues on the FAT filesystem.
- After swapping artwork, reboot the skull and watch for `Bitmap logo printed successfully` in the serial log. If that line is missing, the printer emitted the text fallback instead.

## Current Progress

- Fortune generation now loads mad-libs templates from `/fortunes/little_kid_fortunes.json`, validates required wordlists, and outputs fully populated fortunes (see story-003a build log dated 2025-10-28).
- Thermal printer is integrated with the state machine; fortunes print during `FORTUNE_FLOW` and the serial console echoes the complete text.
- Bitmapped logos stream from SD via ESC/POS raster commands with a centered fallback banner when assets are missing.
- Remaining scope for this story focuses on diagnostics, broader testing, and full-flow validation on hardware.

## Work Log

- **2025-10-29 — Context review**  
  - **What worked**: Read `README.md`, `docs/spec.md`, `docs/hardware.md`, and `docs/stories/story-003a-state-machine-implementation.md`; inspected `src/main.cpp` state machine implementation to confirm current transitions.  
  - **What failed**: n/a  
  - **Lessons learned**: State machine already enforces busy policy and timed transitions; remaining story scope centers on printer logo support and documentation.  
  - **Next steps**: Clarify bitmap logo requirements, audit existing printer assets, and plan implementation/tests for concurrent audio + printing once ready to resume story work.
- **2025-10-29 — Bitmap logo pipeline + docs**  
  - **What worked**: Ported PoC raster command flow into `ThermalPrinter`, added BMP parsing with palette-driven inversion, centered narrow logos, wrapped fortune text, surfaced config-driven logo paths in `setup()`, and built both `esp32dev` + `esp32dev_ota` targets via `pio run`. Documented SD card workflow for fortunes and printer assets.  
  - **What failed**: Could not exercise the printer on hardware in this pass; firmware changes compiled only locally.  
  - **Lessons learned**: The CSN-A1X expects palette index 0 to map to black, so inverting the byte stream keeps ESC/POS semantics consistent; documentation needs to emphasize uncompressed 1-bit BMP exports.  
  - **Next steps**: Flash to the production skull, verify concurrent audio/printing on hardware, and collect printed samples for user sign-off.
- **2025-10-29 — UART pin audit**  
  - **What worked**: Reconfirmed Matter UART pinning lives in `uart_controller.h` (GPIO22 RX, GPIO21 TX) while the thermal printer now rides `Serial2` on GPIO18/19. Pulled the Matter pins out of the controller class and defined them next to the other top-level constants so firmware and docs stay in sync; updated `README.md` and `docs/hardware.md` accordingly with reminders about pin isolation.  
  - **What failed**: n/a  
  - **Lessons learned**: Hiding critical GPIO assignments inside helper classes makes it easy to miss collisions; document the dedicated UART split whenever we touch printer firmware.  
  - **Next steps**: Verify the wiring harness still matches the new docs before field testing.
- **2025-10-29 — Printer self-test command**  
  - **What worked**: Added `ptest` CLI command that routes through a new `ThermalPrinter::printTestPage()` helper; with stable power restored it now triggers the printer’s built-in self-test (DC2 `T`) so the full diagnostic card prints on demand.  
  - **What failed**: Attempting to center the “Your Fortune” header and support inline `\n\n` in templates caused the printer to spew garbage after partial logos; reverted those changes for now. Hardware fortune prints work again but templates must remain single-paragraph until we add a robust renderer.  
  - **Lessons learned**: ESC/POS state can be fragile—mixing custom justification resets with raster output needs careful sequencing/debug time. Document issues before landing formatting tweaks.  
  - **Next steps**: Future fix should (a) reintroduce centered header safely, (b) honor explicit newlines by converting `\n` to manual wraps without confusing the printer, and (c) add regression test fortunes before changing print formatting.
- **2025-10-29 — Bench power rewire**  
  - **What worked**: Rebuilt the 5 V distribution: bench PSU now at 5 V/5 A, negative lead into a 5-slot Wago for all grounds, positive lead split into two 3-slot Wagos (one for the WROVER + SuperMini, one for the printer + servo). After the change, `ptest` prints without brownouts. Documented the wiring in `docs/construction.md`.  
  - **What failed**: n/a  
  - **Lessons learned**: Even with a capable PSU, shared Dupont jumpers introduce enough resistance to drop the rail when the printer fires; proper bus wiring and higher current limits keep the ESP32 stable.  
  - **Next steps**: Keep this distribution in mind when transitioning from bench harness to the final perfboard/power PCB.
