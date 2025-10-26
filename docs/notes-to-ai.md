# Notes to AI

NOTE: This file is for the user to write notes to the AI for it to investigate/record/handle/etc. Once handled, erase the notes from this file as long as they're 100% captured/dealt with. If you are partway through dealing with something, leave the note in this file so you can pick up where you left off, and update the note with the date/time (20251022 12:00:00) and the progress you've made. Do not delete this paragraph.

## NOTES


- We need to remove the TwoSkulls folder once we have everything we need from there. It was just our working baseline, but we're free to rewrite 100% of it. Same with the "proof-of-concept-modules" folder.

- We want a serial interface so we can test functions over the serial port, like the one implemented here: https://github.com/copperdogma/death-poc/blob/master/finger-detector-test/finger-detector-test.ino

- do we NEED to be arduino compatible? why?

MATTER CONTROLLER UPDATES REQUIRED
The PoC Matter controller needs updates to:
- Add Zone Trigger Logic:
  - Far zone occupancy → send TRIGGER_FAR (0x05)
  - Near zone occupancy → send TRIGGER_NEAR (0x06)
- Integrate with Occupancy Sensor: Connect to matter-two-zone-occupancy-sensor for zone detection
- Maintain Existing Protocol: Keep all existing PoC commands (HELLO, SET_MODE, PING, etc.)


QUESTIONS
- if the UART comms fails completely what should we do? currrently we just require a manual reboot. If we can detect failure cases, what should we do in general? How to alert the operator?
