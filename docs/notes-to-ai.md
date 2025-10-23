# Notes to AI

NOTE: This file is for the user to write notes to the AI for it to investigate/record/handle/etc. Once handled, erase the notes from this file as long as they're 100% captured/dealt with. If you are partway through dealing with something, leave the note in this file so you can pick up where you left off, and update the note with the date/time (20251022 12:00:00) and the progress you've made. Do not delete this paragraph.

## NOTES


- 20251022: Codex got the base code working in platformio! But it removed the frame hooks that, I think, were used by the audio analysis code that was used to drive the servo (and eye) animations. That's probably the next most important thing, as it might be a dealbreaker for this codebase and we'd have to revert to the TwoSkulls codebase and start over.

- We need to remove the TwoSkulls folder once we have everything we need from there. It was just our working baseline, but we're free to rewrite 100% of it.

- We want a serial interface so we can test functions over the serial port, like the one implemented here: https://github.com/copperdogma/death-poc/blob/master/finger-detector-test/finger-detector-test.ino

- do we NEED to be arduino compatible? why?


OTA
- DONE: ✅ can we set up the esp32 so we can upload wirelessly? → **PLANNED** - See [ESP32 Wireless Features Plan](.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md) (Phase 1: OTA Upload)
- DONE: ✅ can we set up the esp32 so we can monitor the esp32's serial output wirelessly? → **PLANNED** - See [ESP32 Wireless Features Plan](.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md) (Phase 2: Remote Serial Monitor)
- DONE: Ensure the SD card config includes a non-empty `ota_password` before we wrap the OTA section
- DONE: What's with the I/D/E/W prefixes in the logging? What do they mean? They're not obvious to me. Can we go back to the original logging style with emojis?2
- move the @ota-logging.md file's section on how to flash/reset/monitor/etc the esp32 elsewhere. Where should it be? README.md? an AI rule/file somewhere? Discuss.
- DONE: how do we add the custom OTA commands to the esp32dev_ota platformio "Project Tasks" section? Is that an appropriate thing to do int he first place? My goal is to allow a user to easily see all available OTA commands an run them with one click like the other commands in "Project Tasks"
