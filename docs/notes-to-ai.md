# Notes to AI

NOTE: This file is for the user to write notes to the AI for it to investigate/record/handle/etc. Once handled, erase the notes from this file as long as they're 100% captured/dealt with. If you are partway through dealing with something, leave the note in this file so you can pick up where you left off, and update the note with the date/time (20251022 12:00:00) and the progress you've made. Do not delete this paragraph.

## NOTES

- ✅ can we set up the esp32 so we can upload wirelessly? → **PLANNED** - See [ESP32 Wireless Features Plan](.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md) (Phase 1: OTA Upload)
- ✅ can we set up the esp32 so we can monitor the esp32's serial output wirelessly? → **PLANNED** - See [ESP32 Wireless Features Plan](.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md) (Phase 2: Remote Serial Monitor)

- 20251022: Codex got the base code working in platformio! But it removed the frame hooks that, I think, were used by the audio analysis code that was used to drive the servo (and eye) animations. That's probably the next most important thing, as it might be a dealbreaker for this codebase and we'd have to revert to the TwoSkulls codebase and start over.