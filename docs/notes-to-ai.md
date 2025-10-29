# Notes to AI

NOTE: This file is for the user to write notes to the AI for it to investigate/record/handle/etc. Once handled, erase the notes from this file as long as they're 100% captured/dealt with. If you are partway through dealing with something, leave the note in this file so you can pick up where you left off, and update the note with the date/time (20251022 12:00:00) and the progress you've made. Do not delete this paragraph.

## NOTES

** TEST: add command to drive servo to max and another to min so we can be 100% sure it's not stalling at the limits.

- We need to remove the TwoSkulls folder once we have everything we need from there. It was just our working baseline, but we're free to rewrite 100% of it. Same with the "proof-of-concept-modules" folder.

- I think the touch-to-recalibrate is too quick.. need to be touching for a full 3 seconds I think

- do we NEED to be arduino compatible? why? AI says no, but it might be a bit of a migration.

- SuperMini now sends Hello and ConnectedToAppleHome.. need to serial log when we get these and ACK each of them.
  - `CMD_BOOT_HELLO` (0x0D) is emitted immediately after boot and every second until the WROVER replies with `RSP_BOOT_ACK` (0x90).
  - `CMD_FABRIC_HELLO` (0x0E) is emitted every second after Matter commissioning completes until `RSP_FABRIC_ACK` (0x91) arrives.

- refactor main.cpp... it's huge and bloated, plus there is duplicate code in it

## Hardware To Do List
[x] WROVER up and running
[x] LED working
[x] SD card working
[x] Servo working
[x] Servo breathing animation working
[x] Hooking up to Bluetooth speaker and playing audio
[x] Servo syncing to audio properly within min/max range - I THINK IT IS? Need to see when it's assembled.
[x] USE THE ONBOARD BLOODY SD CARD READER instead of the one I painstakingly hooked up externally;)
[x] SuperMini Matter controller up and running
[x] UART comms working
[x] Thermal printer working
[x] Capacitive sensor working

QUESTIONS
- if the UART comms fails completely what should we do? currrently we just require a manual reboot. If we can detect failure cases, what should we do in general? How to alert the operator?

