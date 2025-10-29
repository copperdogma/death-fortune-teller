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

- Capacitance calibration
  - Initial calibration: What I keep seeing is that it calibrates and then almost immediately starts saying "finger detected!" IDEALLY the threshold should be set where getting CLOSE to the mouth doesn't trigger it, but going inside the mouth at all triggers it. If our threshold is wrong, it "calibrates" and then immediately starts seeing values far enough outside of baseline that it decides it's detected a finger, even though nothing is near. Maybe this is simple to solve.. Maybe we do the calibration, see the existing range recorded during the calibration, and set the threshold for 10% higher. That's it. 
  - maybe if you trigger the recalibration, it should GET you to put your finger in its mouth with the light on, wait 5 seconds, then get you to take it out, then wait 5 sewconds, then measure again. Maybe keep the light on the entire time aside from the "take the finger out now" blinks. When light goes out, calibration complete. Could record audio prompts to help. This will let it set a NEW threshold even if the hardcoded threshold is off. 

## Hardware To Do List
[x] WROVER up and running
[x] LED working
[x] SD card working
[x] Servo working
[x] Servo breathing animation working
[x] Hooking up to Bluetooth speaker and playing audio
[ ] Servo syncing to audio properly within min/max range - I THINK IT IS? Need to see when it's assembled.
[x] USE THE ONBOARD BLOODY SD CARD READER instead of the one I painstakingly hooked up externally;)
[x] SuperMini Matter controller up and running
[x] UART comms working
[ ] Thermal printer working
[ ] Capacitive sensor working

QUESTIONS
- if the UART comms fails completely what should we do? currrently we just require a manual reboot. If we can detect failure cases, what should we do in general? How to alert the operator?

## To Do Later
- newline support in fortunes.. See printer story; it was started

## Bugs
- death-matter-controller: never sends the RSP_FABRIC_ACK message when it's connected to Apple Home. I KNOW it is because I can open Apple Home and send commands and it works, and the WROVER receives them.
- MAYBE: SD shutdown over time? it boots, reads SD, plays audio, responds to commmands.. if it sits for a while I got:
  I/Audio: Queued audio for welcome skit: /audio/welcome/welcome.wav
  E (288570) diskio_sdmmc: sdmmc_read_blocks failed (257)
  E/AudioPlayer: Failed to open audio file: /audio/welcome/welcome.wav