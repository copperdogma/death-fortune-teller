# Death, the Fortune-Telling Skeleton — High-Level Spec (Planning Doc)

20251021: Created via GPT5 convo: https://chatgpt.com/share/68f86abb-3ba0-800a-9a49-471dac32584c

Scope: Single animatronic skull (“Death”) that welcomes kids, reads a finger, prints a fortune, and talks while printing. Triggered entirely via Matter devices in Apple Home. This doc captures requirements, constraints, interfaces, and build/test plans. No code.

⸻

0) Repos (final vs PoCs)

Final repos (target structure)
	•	death-fortune-teller — main skull firmware (fork of TwoSkulls + new modules).
	•	death-fortune-teller-matter — skull control Matter node (buttons/modes → UART). (Derived from esp32-esp32-matter-link PoC.)
	•	matter-two-zone-occupancy-sensor — dual-zone (far/near) Matter occupancy sensor (existing).

PoCs used as source material
	•	Full PoC collection: https://github.com/copperdogma/death-poc
	•	Skull Matter link PoC: https://github.com/copperdogma/death-poc/tree/master/esp32-esp32-matter-link
	•	Finger detector PoC: https://github.com/copperdogma/death-poc/tree/master/finger-detector-test
	•	Thermal printer PoC: https://github.com/copperdogma/death-poc/tree/master/thermal-printer-test

Base project for fork
	•	TwoSkulls (audio, A2DP, jaw, SD, skits): https://github.com/copperdogma/TwoSkulls

Repo strategy
	•	Keep separate repos (each device stands alone).
	•	Copy PoC code into death-fortune-teller as modules (no Git submodules).
	•	We’ll note final commit SHAs in READMEs for traceability (no pinning mechanics needed).

⸻

1) Hardware & System Overview

Main MCU (Skull): ESP32 WROOM/WROVER (classic ESP32)
	•	Responsibilities: SD WAV playback over A2DP, jaw servo sync via audio analysis, mouth LED, one eye (physically one connected, firmware still exposes both), capacitive finger sensor, thermal printer, UART link to skull Matter node.

Matter devices (separate MCUs):
	•	Two-zone occupancy sensor (Matter): exposes far and near occupancy in Apple Home.
	•	Skull controller (Matter): exposes mode toggles and two triggers (far/near). Sends UART commands to the skull MCU.

Audio path: SD → WAV → A2DP → hidden BT speaker.
Jaw safety: Servo opens only; elastic band closes (“snap” = release).
Printing: 58 mm TTL thermal printer (PoC defaults). Print logo + generated fortune while Death keeps speaking.
Power: Sized so printing + talking + servo run concurrently (≥3 A @ 5 V rail for printer, with local bulk cap).

⸻

2) Modes, Triggers, and Behavior

Modes (future; may slip this year): LittleKid (default for 2025 MVP), BigKid, TakeOne, Closed.
	•	For 2025: run LittleKid only. Mode changes received while speaking are ignored (no queue).

Triggers (always via Matter automations):
	•	Far trigger → play a Welcome skit.
	•	Near trigger → run the Fortune flow (prompt, mouth open, finger read, snap, print, continue talking).

Busy policy (critical):
	•	While any skit is playing (or snap/print in progress), ignore and drop all incoming commands (triggers, mode changes).
	•	No auto-chaining: If NEAR arrives during WELCOME, drop it. After WELCOME ends and system returns IDLE, a new NEAR trigger is required.

Debounce/precedence:
	•	Duplicate trigger debounce on skull: 2 s.
	•	If idle and multiple triggers arrive nearly simultaneously, first processed wins; subsequent triggers are dropped until next IDLE.

⸻

3) State Machine (runtime)

IDLE
 ├─ on TRIGGER_FAR  → PLAY_WELCOME  → (end) → IDLE
 └─ on TRIGGER_NEAR → FORTUNE_FLOW  → (end) → COOLDOWN → IDLE

FORTUNE_FLOW:
  PLAY_PROMPT → MOUTH_OPEN → MOUTH_LED pulse
  WAIT_FINGER (max 6s; requires ≥120ms solid detection)
    ├─ finger detected → start random 1–3s timer → SNAP (release) → PRINT_FORTUNE → continue monologue → end
    └─ timeout → abort (close mouth) → end

Timers (initial):
	•	Finger solid detection: ≥120 ms before starting the snap timer.
	•	Snap delay: random 1–3 s (snap even if the finger leaves during this delay).
	•	Finger wait timeout: 6 s.
	•	Post-fortune cooldown: 12 s.

(Welcome duration is an asset property; there is no “wait-near window” in MVP.)

⸻

4) Assets & File Layout

SD layout (proposed):

/audio/
  /welcome/          # welcome skits (WAV)
    welcome_01.wav
    welcome_02.wav
  /fortune/          # fortune skits (WAV)
    fortune_01.wav
    fortune_02.wav
  Initialized.wav    # boot self-test line

/printer/
  logo_384w.bmp      # 1-bit, 384px wide
  fortunes_littlekid.json

/config/
  config.txt         # see §8

Skit selection:
	•	Weighted random; no immediate repeat (same as TwoSkulls).
	•	Adapts to whatever files exist (1..N).

Fortune content (JSON; validated on boot)

{
  "version": 1,
  "templates": [
    "Your fortune speaks of {{theme}} — expect {{event}} near {{place}}.",
    "Beware {{omen}}; yet {{boon}} follows if you {{verb}}."
  ],
  "wordlists": {
    "theme": ["treasure","adventure","luck"],
    "event": ["a surprise","a test","a helper"],
    "place": ["the crossroads","the old gate","the red door"],
    "omen": ["shadows","a crow","fog"],
    "boon": ["a charm","good tidings","a secret path"],
    "verb": ["listen","wait","share"]
  }
}

Validation rules:
	•	Required keys: version (int), templates (non-empty array of strings), wordlists (object).
	•	Every {{token}} in every template must exist in wordlists[token] with ≥1 entry.

⸻

5) Audio, Jaw Sync, and Typewriter SFX

Sample rate: same as TwoSkulls (16-bit PCM, 22.05/44.1 kHz).
Jaw sync: real-time amplitude analysis of the speech signal only.

During printing we must keep the jaw talking. Two implementation options (decide during integration):
	1.	Stereo trick (preferred if supported)
	•	Make stereo WAV: Left = speech, Right = typewriter.
	•	Send stereo over A2DP; run jaw analysis on Left only.
	2.	Live mix (more code)
	•	Play two PCM streams (speech + typewriter), mix in software to output; analyze speech stream only.

(We explicitly will not mute jaw during typewriter.)

⸻

6) UART Link (Skull Matter Node → Skull Main)

Transport (from PoC you provided):
	•	UART1 at 115200 8N1, TX=21, RX=20, RX buf=1024.
	•	Frame format: 0xA5 (start), then [LEN][CMD][PAYLOAD...][CRC8]
	•	LEN = 1 + payload_len (the “1” accounts for CMD)
	•	CRC-8 poly 0x31, init 0x00, computed over LEN+CMD+PAYLOAD.

Commands used in Death (sender = skull Matter node):
	•	0x02 SET_MODE {u8 mode} — (future) 0=LittleKid, 1=BigKid, 2=TakeOne, 3=Closed
	•	0x05 TRIGGER_FAR — no payload
	•	0x06 TRIGGER_NEAR — no payload
	•	0x04 PING — optional for diagnostics only

Commands explicitly not required for MVP: 0x03 legacy TRIGGER (unused).

Directionality & robustness
	•	One-way intended use: Matter node sends, skull does not reply (no ACK/NAK expected).
	•	Skull debounces duplicates (2 s) and drops while busy.
	•	Matter node may spam (Apple Home jank) — acceptable; skull side is authoritative.

⸻

7) Peripherals & Signals
	•	Jaw servo: open-only; snap = release. Config in microseconds (min/mid/max) to ease per-servo calibration.
	•	Capacitive finger: boot-time calibration (mouth open, quiet electronics). Detection requires ≥120 ms solid.
	•	Mouth LED: on during prompt; slow pulse while waiting for finger.
	•	Eyes: firmware still drives EYES (two channels/patterns). For 2025, one eye is physically disconnected; keep optional header for future.
	•	Printer: 58 mm TTL; use PoC defaults (baud/density/feed). Print logo then fortune. If printer error (paper/overtemp/absent), play a global fallback line.
	•	Power: 5 V rail ≥3 A for printer + 1000–2200 µF bulk near the printer; separate/stout wiring; common ground.

⸻

8) config.txt (initial keys)

speaker_name=FortuneSkull
default_mode=LittleKid           # (modes may be ignored in 2025 MVP)
a2dp_target=YourSpeakerName

# Jaw servo (microseconds)
servo_us_min=500
servo_us_mid=1500
servo_us_max=2500
jaw_open_us=...                  # to be tuned
jaw_snap_style=release           # fixed

# Mouth LED
mouth_led_pin=...
mouth_led_idle=0
mouth_led_prompt=255
mouth_led_pulse=1

# Cap sense
cap_pin=...                      # or touch channel
cap_threshold=...                # discovered/overridden at boot
cap_hysteresis=...

# Timing
finger_wait_ms=6000
snap_delay_min_ms=1000
snap_delay_max_ms=3000
cooldown_ms=12000

# Printer
printer_uart=1
printer_baud=9600                # PoC default
printer_logo=/printer/logo_384w.bmp
fortunes_json=/printer/fortunes_littlekid.json

(Pins to be assigned per your perfboard; these keys are placeholders.)

⸻

9) Build & Tooling (Cursor + PlatformIO)

We will build skull firmware with PlatformIO (Arduino framework).
	•	Initial board target: esp32dev (classic ESP32)
	•	If memory/link issues appear, switch to esp-wrover-kit and enable PSRAM (-DBOARD_HAS_PSRAM).
	•	Partitions: use Huge App (3 MB app).
	•	ESP32 Arduino core: match your known-good (≈ 2.0.17). Choose a platform-espressif32 version that maps to this core; freeze after bring-up.
	•	Libraries: ESP32-A2DP, arduinoFFT, SD (+ any WAV decoder used in TwoSkulls).
	•	Monitor: monitor_speed = 115200.

Why PIO will work now: issues you hit before were almost certainly partition/core/board selection, not PIO itself.

⸻

10) Test Console & Diagnostics

Serial test console commands (baseline):
	•	play welcome
	•	play fortune
	•	mouth open / mouth close
	•	print test
	•	calibrate cap
	•	set mode <0..3> (for future)
	•	show status

LED fault cue: 3 quick blinks for printer faults (in addition to serial logs).

⸻

11) Bring-Up Plan (MVP in this order)
	1.	PIO + Board + Partitions: build and flash a stub; confirm serial.
	2.	SD + A2DP + WAV: mount SD, play Initialized.wav to BT speaker reliably.
	3.	Jaw sync: run FFT-driven jaw on the WAV (plausible motion).
	4.	UART: accept TRIGGER_FAR/NEAR frames (no responses). Confirm debounce and busy drop.
	5.	Cap sense: boot calibration, detect ≥120 ms solid.
	6.	Mouth LED: prompt on, pulse while waiting.
	7.	Snap: random 1–3 s after solid finger detection, release only.
	8.	Printer: print logo + sample fortune; confirm while A2DP playing.
	9.	Skits: welcome directory + fortune directory; weighted random; no immediate repeat.
	10.	Home automations: wire far → TRIGGER_FAR, near → TRIGGER_NEAR; confirm busy-drop behavior aligns with expectations.

Pass criteria: No crashes; audio solid; servo motion natural; printing during speech stable; triggers behave per spec.

⸻

12) Open Decisions (choose during integration)
	•	Typewriter strategy:
Option 1: Stereo trick (Left speech / Right typewriter; analyze Left).
Option 2: Live mix (two PCM streams; analyze speech; more code).
	•	Exact board target & core version: freeze after bring-up passes.

⸻

13) Risks & Mitigations
	•	Apple Home jank / trigger spam → Skull debounces and drops while busy; users may need to re-tap when idle.
	•	Printer power dips → ≥3 A 5 V rail + local bulk cap; short, thick leads; separate return path.
	•	A2DP latency/BT hiccups → keep assets modest; avoid excessive CPU spikes during print; PSRAM if needed.
	•	Audio channeling for jaw → prefer stereo trick; fall back to live mix if needed.
	•	Core/partition mismatch → lock Huge App + known-good core once stable.

⸻

14) Acceptance Checklist (MVP)
	•	Plays Initialized.wav to your BT speaker on boot.
	•	TRIGGER_FAR causes a welcome skit; NEAR during welcome is ignored; returns to IDLE.
	•	TRIGGER_NEAR runs full fortune flow: prompt → mouth open → LED pulse → finger detect → random 1–3 s → snap → print while speaking → cooldown.
	•	Fortunes generated from validated JSON; logo prints first.
	•	Busy policy enforced (everything is dropped while active).
	•	Debounce works (duplicate triggers within 2 s are ignored).
	•	Single-eye hardware works; firmware still drives EYES.
	•	Serial test console + printer fault LED blinks work.

⸻

15) Links (for Cursor to reference)
	•	TwoSkulls (base): https://github.com/copperdogma/TwoSkulls
	•	Death PoCs (root): https://github.com/copperdogma/death-poc
	•	Matter link PoC: https://github.com/copperdogma/death-poc/tree/master/esp32-esp32-matter-link
	•	Finger detector PoC: https://github.com/copperdogma/death-poc/tree/master/finger-detector-test
	•	Thermal printer PoC: https://github.com/copperdogma/death-poc/tree/master/thermal-printer-test
	•	Two-zone occupancy sensor: https://github.com/copperdogma/matter-two-zone-occupancy-sensor

⸻

Appendix A — UART constants from your PoC
	•	UART1 @ 115200 8N1, TX=21, RX=20, RX buf=1024.
	•	Frame: 0xA5 [LEN][CMD][PAYLOAD...][CRC8], LEN = 1 + payload_len.
	•	CRC-8: poly 0x31, init 0x00, over LEN+CMD+PAYLOAD.
	•	Commands present in PoC:
HELLO=0x01, SET_MODE=0x02, TRIGGER=0x03 (legacy), PING=0x04; statuses 0x10/0x11; responses 0x80–0x83.
	•	Death additions for clarity:
TRIGGER_FAR=0x05, TRIGGER_NEAR=0x06 (no payload).
(Matter node uses these; skull expects no replies to be read by the node.)
