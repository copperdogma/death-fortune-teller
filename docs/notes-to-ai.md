# Notes to AI

NOTE: This file is for the user to write notes to the AI for it to investigate/record/handle/etc. Once handled, erase the notes from this file as long as they're 100% captured/dealt with. If you are partway through dealing with something, leave the note in this file so you can pick up where you left off, and update the note with the date/time (20251022 12:00:00) and the progress you've made. Do not delete this paragraph.

## NOTES


- We need to remove the TwoSkulls folder once we have everything we need from there. It was just our working baseline, but we're free to rewrite 100% of it.

- We want a serial interface so we can test functions over the serial port, like the one implemented here: https://github.com/copperdogma/death-poc/blob/master/finger-detector-test/finger-detector-test.ino

- do we NEED to be arduino compatible? why?

- put back breathing animation cycle

- make an ai-work/issues folder and have the Ai start an issues file for every issue it has to tackle. This will be a nice log and help future AIs to pick up where we left off and/or make sure they don't repeat the same mistakes/failre-bound attempts. Make a slash command to go with it. And SHOULD we just keep one issues file per topic, or one per issue we need to tackle? Discuss.

MATTER CONTROLLER UPDATES REQUIRED
The PoC Matter controller needs updates to:
- Add Zone Trigger Logic:
  - Far zone occupancy → send TRIGGER_FAR (0x05)
  - Near zone occupancy → send TRIGGER_NEAR (0x06)
- Integrate with Occupancy Sensor: Connect to matter-two-zone-occupancy-sensor for zone detection
- Maintain Existing Protocol: Keep all existing PoC commands (HELLO, SET_MODE, PING, etc.)


QUESTIONS
- if the UART comms fails completely what should we do? currrently we just require a manual reboot. If we can detect failure cases, what should we do in general? How to alert the operator?


OTA
- DONE: ✅ can we set up the esp32 so we can upload wirelessly? → **PLANNED** - See [ESP32 Wireless Features Plan](.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md) (Phase 1: OTA Upload)
- DONE: ✅ can we set up the esp32 so we can monitor the esp32's serial output wirelessly? → **PLANNED** - See [ESP32 Wireless Features Plan](.cursor/plans/esp32-wireless-ota-upload-58791e9e.plan.md) (Phase 2: Remote Serial Monitor)
- DONE: Ensure the SD card config includes a non-empty `ota_password` before we wrap the OTA section
- DONE: What's with the I/D/E/W prefixes in the logging? What do they mean? They're not obvious to me. Can we go back to the original logging style with emojis?2
- move the @ota-logging.md file's section on how to flash/reset/monitor/etc the esp32 elsewhere. Where should it be? README.md? an AI rule/file somewhere? Discuss.
- DONE: how do we add the custom OTA commands to the esp32dev_ota platformio "Project Tasks" section? Is that an appropriate thing to do int he first place? My goal is to allow a user to easily see all available OTA commands an run them with one click like the other commands in "Project Tasks"
