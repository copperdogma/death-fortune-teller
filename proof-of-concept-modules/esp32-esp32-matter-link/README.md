# ESP32 Matter Master/Slave POC

**Status: ✅ COMPLETE**

This is a **working proof-of-concept** for building Matter-enabled devices using a master/slave architecture with two ESP32 boards communicating over UART.

## 👉 **[START-HERE.md](START-HERE.md)** - Navigation guide for AI agents and humans

## 📖 Documentation (Start Here!)

### For AI Agents Building Production Projects:
1. **[POC-SUMMARY.md](POC-SUMMARY.md)** ⭐ - **READ THIS FIRST** - Complete technical reference (400+ lines)
   - All patterns and code examples
   - Every gotcha and workaround discovered
   - Performance data and debugging tips
   
2. **[QUICK-REFERENCE.md](QUICK-REFERENCE.md)** - Copy-paste code patterns
   - UART protocol implementation
   - Mutual exclusivity pattern
   - Critical gotchas with examples

3. **[CHECKLIST.md](CHECKLIST.md)** - Implementation verification
   - Step-by-step checklist for building similar projects
   - Testing procedures
   - Success criteria

### For Environment Setup:
4. **[esp32-matter-node/SETUP.md](esp32-matter-node/SETUP.md)** - ESP-IDF and ESP-Matter installation

### Historical Reference:
5. **[Original ChatGPT Convo](https://chatgpt.com/share/68ebfd66-49dc-800a-a080-06e2c2ba1a30)** - Initial project discussion (20251012)
6. **[esp32-matter-node/docs/matter-mode-research/](esp32-matter-node/docs/matter-mode-research/)** - Research on HomeKit mode selection options

## 🎯 Quick Start

1. **Flash ESP32-S3** (Master): Open `esp32-matter-master/esp32-matter-master.ino` in Arduino IDE
2. **Flash ESP32-C3** (Matter Node): See build instructions in POC-SUMMARY.md
3. **Wire boards**: TX↔RX crossover, shared GND, independent power
4. **Test UART**: Use S3 CLI commands (`hello`, `ping`, `trigger`, `mode <0-3>`)
5. **Add to HomeKit**: Scan QR code `MT:Y.K90GSY00KA0648G00`
6. **Rename outlets**: Trigger, Little Kid, Big Kid, Take One, Closed

## 🎓 What You'll Learn

- ✅ Bidirectional UART with CRC8 between ESP32 boards
- ✅ Matter device integration with HomeKit
- ✅ Multiple endpoint management
- ✅ Mutual exclusivity patterns for mode selection
- ✅ Debouncing and state management
- ✅ HomeKit quirks and workarounds
- ✅ Factory reset and commissioning

## ⚠️ Known HomeKit Limitations

1. **UI Caching**: Rapid mode changes may require force-closing Home app
2. **Manual Naming**: Device and outlets must be renamed after pairing
3. **Update Delays**: UI can take 1-10 seconds to reflect state changes

These are **HomeKit limitations**, not code issues. See POC-SUMMARY.md for details.

---

*Original Project Context (20251012):*




🎃 Project: Death Fortune Teller — Matter-Enabled Animatronic Skeleton

1️⃣  Overview

We are building a proof of concept connecting a Freenove ESP32-S3-WROOM (main controller) and an ESP32-C3 SuperMini (Matter controller).
The goal is to make the animatronic skeleton visible and controllable in Apple Home (via Matter) while maintaining all skit timing and logic on the S3.

⸻

2️⃣  Functional roles

Board	Role	Responsibilities	Radio Use
ESP32-S3-WROOM	Main brain (Director)	• Runs skits (audio, servos, LEDs).• Reads SD card scripts.• Generates sound via Bluetooth A2DP to JBL Flip speaker.• Controls jaw via FFT.• Communicates with C3 via UART (simple command protocol).	Bluetooth Classic (A2DP) only — no Wi-Fi.
ESP32-C3 SuperMini	Matter controller (Runner)	• Joins the same Matter fabric as the other Halloween props.• Receives skit commands from S3 via UART.• Issues Matter commands (On/Off, Level, Groups) to other devices (e.g., crow, blacklight).• Exposes its own Matter endpoints so the fortune teller can be triggered or have its mode set from any Matter controller (e.g., iPhone Home app).	Wi-Fi/BLE for Matter — no A2DP.


⸻

3️⃣  Primary objective (Proof of Concept)

Implement bidirectional UART communication between the S3 and the C3, where:
	•	The C3 exposes a Matter device with:
	1.	An On/Off Plug-in Unit endpoint → triggers a skit (sends UART signal to S3).
	2.	A mode selector allowing four distinct modes:
	•	Little Kid Mode
	•	Big Kid Mode
	•	“We’re Out; Take One” Mode
	•	“Closed for the Night”
	•	The S3 listens for trigger and mode commands from the C3 and responds over UART with ACKs or error codes.

⸻

4️⃣  Desired HomeKit / Matter UI behavior

When commissioned, the device should appear in Apple Home (and any Matter controller) as:
	1.	Tile #1: “Fortune Teller Trigger” — a standard on/off switch.
	•	Turning ON triggers the skit and auto-resets to OFF after ~500 ms.
	2.	Tile #2: “Fortune Teller Mode” — one of:
	•	(Option A) Dimmable light with 4 brightness ranges mapping to the four modes.
	•	(Option B) Four separate switches labeled with each mode, one-hot enforced.

The user can set the mode from the Home app or automations; the selection is stored in NVS and reported to the S3 over UART.

⸻

5️⃣  UART protocol (between S3 and C3)

Baud: 115200, 8N1, no flow control.
Lines: TX, RX, GND shared.

Frame format

0xA5 LEN CMD PAYLOAD... CRC8

Commands C3 → S3

CMD	Description	Example payload
HELLO	Identify handshake	none
SET_MODE	Sets operating mode	1 byte (0–3)
TRIGGER	Start skit	none
PING	Health check	none

Responses S3 → C3

CMD	Meaning
ACK	Success
ERR	Error code
BUSY	Skit already running
DONE	Skit finished

CRC8 = Dallas/Maxim polynomial (0x31).

⸻

6️⃣  Matter device specification (ESP-C3)

Endpoint 1 — Trigger
	•	Device type: On/Off Plug-in Unit (0x010A).
	•	Clusters: OnOff.
	•	Behavior:
	•	On OnOff = true → send TRIGGER frame to S3, then auto-report OnOff = false after 500 ms.
	•	Ignore subsequent triggers if S3 reports BUSY.

Endpoint 2 — Mode selection

Two equivalent implementation options:

Option A — Dimmable Light
	•	Device type: Dimmable Light (0x0101).
	•	Clusters: OnOff, LevelControl.
	•	Mapping:

Brightness	Mode
1–24%	Little Kid
25–49%	Big Kid
50–74%	Take One
75–100%	Closed


	•	Quantize any slider value to these buckets and send SET_MODE to S3.

Option B — Four separate switches
	•	Four On/Off endpoints (0x010A).
	•	Turning one ON turns the rest OFF.
	•	Each sends SET_MODE n to S3.

(Option A keeps 2 tiles total; Option B is most explicit in UI.)

⸻

7️⃣  Future expansion (Phase 2)
	•	Extend UART protocol so S3 can send back:
	•	ENUM_REQ → C3 replies with visible Matter devices and online status.
	•	CMD id action → C3 issues Matter commands to other props (e.g., crow ON, blacklight ON).
	•	ACK/ERR → confirms command success/failure.

This will let the S3 orchestrate external Matter props while keeping audio timing local.

⸻

8️⃣  Hardware wiring

Signal	From	To	Notes
3V3	C3	S3	Shared regulated rail
GND	C3	S3	Common ground
TX	C3	RX (S3)	UART command line
RX	C3	TX (S3)	UART response line

Optional: EN or RESET if synchronized reboot needed.

⸻

9️⃣  Libraries / frameworks
	•	ESP32-S3: Arduino core + existing BluetoothA2DPSink + HardwareSerial.
	•	ESP32-C3: esp-matter framework (Espressif’s official SDK).
	•	Internal pull-downs on UART pins to avoid float triggers.

⸻

10️⃣  Proof-of-Concept success criteria
	1.	Both boards boot and perform UART handshake (HELLO/ACK).
	2.	Matter device appears in Apple Home with two controls (Trigger + Mode).
	3.	Turning the trigger ON causes the S3 to start a placeholder skit (e.g., LED blink, serial log).
	4.	Changing the mode slider/switch updates S3 via UART and is confirmed by serial output.
	5.	The device remains visible and controllable after power cycle (mode persisted in NVS).

⸻

✅ Summary:
Build a two-ESP32 proof of concept where the ESP32-C3 hosts a Matter node with an On/Off trigger and 4-state mode selector, relaying commands to an ESP32-S3 over UART.
The S3 runs the “Death Fortune Teller” skit logic and will later direct other Matter props through the C3.
Keep the C3 completely responsible for Matter networking; keep all audio and real-time work on the S3.