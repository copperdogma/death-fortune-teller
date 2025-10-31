# Perfboard Assembly Guide (Final Build)

This is the step‑by‑step guide to solder the final Death Fortune Teller electronics onto a perfboard and verify each subsystem as you go. It matches the final hardware decisions in docs/hardware.md as of 2025‑10‑29.

Key choices locked
- Power input: 5 V 5 A adapter into a 2.1 mm panel‑mount barrel jack (center‑positive).
- Back‑feed protection: Series Schottky diode at the main 5 V input (protects the computer during USB programming).
- SD card: Built‑in WROVER slot, SD_MMC 1‑bit @ 20 MHz.
- ESP32‑C3 power: From WROVER 3V3 (more efficient, small draw).
- LEDs: Single 3‑pin connector (Eye=GPIO32, Mouth=GPIO33, GND) with 100 Ω series resistors.
- Printer cable: ~4 ft Ethernet cable (twisted pairs), 9600 baud.
- Extra decoupling: 1000–2200 µF at printer branch, 470–1000 µF at servo branch.

Color key and wire gauges
- 5 V = red, 3.3 V = orange, GND = black
- TX out of a device = green, RX into a device = yellow
- LED signals = purple (I actually used red), capacitive sense = white
- Barrel jack to board and ground bus: 18–20 AWG
- 5 V branches: 20–22 AWG; signals: 24–26 AWG

Required parts
- Perfboard (BusBoard PR1590BB or similar dual‑pad board)
- Schottky diode ≥3 A (preferably ≥5 A for headroom), e.g., 1N5822 (min) or SR560 (5 A class)
- Electrolytics: 1000–2200 µF (printer), 470–1000 µF (servo), ≥10 V rating
- Panel‑mount 2.1 mm barrel jack (center‑positive)
- Headers: female for WROVER and C3 (or direct solder if preferred), 3‑pin LED connector, 3‑pin servo header
- Coax for capacitive sensor (shield to GND at board end only) + copper foil electrode + copper foil shield
- Ethernet cable (Cat5/5e) for the printer harness
- Dupont/JST as needed (printer uses JST at the device end)

Bench checklist before soldering
- CONFIRMED: Confirm barrel jack polarity (center = +5 V)
- Schottky orientation: band toward the 5 V bus
- Have a multimeter ready (DC voltage + continuity)
- Have the SD card prepared with the audio tree and config.txt

Tips
- Keep high‑current paths short and thick (barrel jack ↔ diode ↔ 5 V bus ↔ printer/servo branches).
- Run signal wiring away from the servo and printer power leads where possible.
- Tie all returns to the single ground bus; avoid daisy‑chaining grounds through loads.
- VCC/GND on paired-island perfboard:
	•	Dedicate one full horizontal pair-row to 5 V
	•	The adjacent horizontal pair-row directly below it to GND
	•	You can then drop capacitors right between paired holes like this:
    (Top view of pads)
        5V ● ● ● ● ● ●
          |   |   |     |<— capacitors drop vertically between rows
        GND ● ● ● ● ● ●

Stage 1 — Power buses (build and test)
1) Ground bus
- Solder a short, thick ground bus along one edge of the perfboard (18–20 AWG). This is the single return for all grounds.

1) 5 V input and diode
- Wire barrel jack (+) to Schottky anode; Schottky cathode (banded end) to the 5 V bus.
- Barrel jack (−) to the ground bus.

1) 5 V branches and bulk caps
- Create two 5 V branch takeoffs from the bus: Printer and Servo. Keep them physically separated.
- Printer branch: add 1000–2200 µF (+: branch, −: ground bus).
- Servo branch: add 470–1000 µF (+: branch, −: ground bus).

1) Power sanity test
- With no loads connected, apply the 5 V adapter. Measure:
  - Barrel jack + to ground: ~5.0 V
  - 5 V bus after the diode: ~4.6–4.9 V (drop depends on diode and current — minimal at no‑load)
- Remove power.

Stage 2 — Mount controllers
1) ESP32‑WROVER
- Mount the WROVER on headers. Connect 5 V bus (after diode) to VIN/5V; GND to ground bus.
- Mark/label GPIO18, 19, 21, 22, 23, 32, 33 for easy routing.

2) ESP32‑C3 SuperMini
- Mount near the WROVER. Power it from the WROVER’s 3V3 (orange) and ground (black).
- Cross‑connect UART for Matter link:
  - WROVER GPIO21 (TX) → C3 GPIO20 (RX) — green
  - WROVER GPIO22 (RX) ← C3 GPIO21 (TX) — yellow

3) Controller bring‑up tests
- WROVER quick test
  - Apply 5 V adapter; plug USB into the WROVER for serial. Safe to program with external power thanks to the input diode.
  - Verify boot logs; confirm SD_MMC mounts `/sdcard` and the audio directory scan prints in serial. If not, fix now.
  - Pair Bluetooth speaker and confirm A2DP connects (logs will show connection). Play the Initialized.wav if configured.
- SuperMini quick test
  - Verify C3 3V3 is present (from WROVER) and that its serial console/LED heartbeat is alive (if firmware provides it).
  - Confirm it appears in Apple Home (if already commissioned) and can trigger actions.
  - Trigger the FAR/NEAR automations and watch WROVER serial for received UART frames (e.g., FAR_MOTION_DETECTED/NEAR_MOTION_DETECTED).

Stage 3 — LEDs and sensor
1) LED connector (3‑pin)
- Add two 100 Ω resistors near the WROVER:
  - GPIO32 → 100 Ω → LED connector pin 1 (Eye) — purple
  - GPIO33 → 100 Ω → LED connector pin 2 (Mouth) — purple
  - Ground bus → LED connector pin 3 (GND) — black

2) Capacitive sensor
- Build the copper “sandwich” (mechanical)
  - Outer foil: ~3×4 cm copper foil adhered to the skull’s upper palate (mouth side).
  - Inner foil: ~4×5 cm copper foil adhered to the inside of the skull, directly behind the outer foil (slightly larger perimeter).
  - Drill a small hole through outer foil → plastic → inner foil to pass the coax center conductor.
- Coax wiring (electrical)
  - Keep the coax shielded run as long as practical (≈4" from skull interior to perfboard).
  - At the skull: solder the coax center conductor through the hole to the outer foil (sensor electrode).
  - At the skull: solder the coax shield to the inner foil (shield plane). Do not run a separate ground wire from the inner foil.
  - At the perfboard: connect coax center to GPIO4 (white). Connect the coax shield to the ground bus (black). This provides a single‑point ground for the inner foil via the shield.
  - Result: the inner foil acts as a grounded barrier to push the field outward into the mouth, while avoiding loops.
  - Optional: add strain relief on the coax at the skull entry and perfboard.
 - LED/sensor test
  - Load a quick LED fade and cap‑sense test (or use the project’s CLI). Confirm PWM control and stable detection.

3) LED/sensor test
- Load a quick LED fade and cap‑sense test (or use the project’s CLI). Confirm PWM control and stable detection.

Stage 4 — Servo
- Add a standard 3‑pin servo header:
  - Signal: GPIO23 — white/orange
  - +5 V: Servo branch — red
  - GND: Ground bus — black
- Test a sweep; verify no brownouts and smooth motion. The servo cap should tame stalls.

Stage 5 — Thermal printer and long cable
1) Make the Ethernet harness (~4 ft)
- Pairing (recommended color map):
  - Orange pair: WROVER TX (GPIO18, yellow) twisted with GND (black)
  - Blue pair:   WROVER RX (GPIO19, green) twisted with GND (black)
  - Green pair:  +5 V supply (both wires paralleled)
  - Brown pair:  GND return (both wires paralleled)
- This maximizes current capacity on power and improves noise immunity on UART.
- Printer ends and connectors
  - The printer typically exposes two JST connectors:
    - 2‑pin JST: +5 V and GND (power)
    - 4‑pin JST: DTR, TXD (printer → ESP), RXD (ESP → printer), GND (data)
  - We do not use DTR in this design. Grounds are common.
  - Build two short adapter pigtails at the printer: one 2‑pin JST for power and one 4‑pin JST for data (leave DTR unconnected).
  - Splice these pigtails to the Ethernet cable as mapped above.
  - At the perfboard end, terminate into a Dupont header (TX, RX, +5 V, GND) so you avoid JST on the board side.

2) Connect to perfboard
- GPIO18 (TX) → printer RXD (yellow)
- GPIO19 (RX) ← printer TXD (green)
- Printer +5 V → printer branch (red); GND → ground bus (black)

3) Printer test
- Power on and run a 9600 baud test print. Check for clean output. If characters garble, re‑terminate, verify pairing (TX↔RX), and ensure pairs are twisted with GND.

Stage 6 — SD card and integration
1) SD card
- Insert into the WROVER’s underside slot. The firmware uses SD_MMC in 1‑bit mode @ 20 MHz (D1/D2 unused), which keeps GPIO4 free for the sensor.

2) Full system test
- Boot with the prepared SD card. Confirm audio directories are detected, Bluetooth connects, servo behaves, and fortunes print during playback.


Reference pin map (final)
- LEDs: GPIO32 (Eye), GPIO33 (Mouth), both via 100 Ω
- Servo: GPIO23 (signal), +5 V (servo branch), GND
- Capacitive sensor: GPIO4 (coax center), shield to GND at board end only
- Printer (UART2): GPIO18 TX → printer RXD; GPIO19 RX ← printer TXD; 5 V (printer branch), GND
- Matter link (UART1): GPIO21 TX → C3 GPIO20 RX; GPIO22 RX ← C3 GPIO21 TX
- SD card: built‑in WROVER slot, SD_MMC 1‑bit @ 20 MHz

If you ever need more SD throughput
- Move the capacitive sensor to another touch pin (e.g., GPIO27) and switch SD_MMC to 4‑bit mode in firmware.
- Be mindful that 4‑bit SDMMC uses GPIO4 (D1) and GPIO12 (D2); keep those free.

Programming safety with external power
- With the series Schottky at the main 5 V input, it’s safe to plug in USB while the 5 V adapter is on. The diode prevents back‑feed to the computer.

Optional enhancements
- Test points: Add small wire loops at 5 V bus (after diode), 3V3 (WROVER), printer branch 5 V, servo branch 5 V, and GND for easy probing.
- Power indicator: Add a 5 V rail LED (e.g., LED + 1 kΩ to 5 V after the diode) for a ~2–3 mA visual power‑good indicator.
- Future SD throughput: If you ever switch to 4‑bit SDMMC, move the capacitive sensor from GPIO4 to GPIO27 and update `CAP_SENSE_PIN` in `src/main.cpp` accordingly.

Done — your perfboard build matches the tested prototype and is ready for installation in the skull and typewriter.
