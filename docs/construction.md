# Physical Construction

## TODO
- full BOM at the top of the file

## Bill of Materials

** NOTE: **DIODE** We'll need a diode to keep power from flowing backwards, When the ESP32 WROVER or SuperMini is plugged in to reprogram or monitor, we don't want it to backdflow and try to power the rest of the system.

### Core Microcontrollers
- **FREENOVE ESP32-WROVER Development Board**: Main controller with Wi-Fi/Bluetooth and PSRAM
- **ESP32-C3 SuperMini**: Matter controller with Wi-Fi/Bluetooth 5 (LE)

### Audio System
- **Bluetooth Speaker**: Hidden speaker for A2DP audio output
- **MicroSD Storage**: Built-in ESP32-WROVER slot (underside, SDMMC)
- **SD Card**: MicroSD card with audio files and configuration

### Visual System
- **Eye LEDs**: 5mm LED for eye (just one; it's a pirate!) - colour TB
- **Mouth LED**: 5mm LED for mouth - colour TBA

### Motion System
- **Servo Motor**: Hitec HS-125MG (4.8v-6v, 3.0-3.5kg.cm torque, 1.2A stall current)

### Sensor System
- ~10x10cm copper foil for capacitive sensor
- coax (shielded) cable for capacitive sensor

### Printing System
- **Thermal Printer**: CSN-A1X or compatible 58mm thermal printer

### Power System
- **System Power**: 6 Pin USB-C female socket
- **5V Regulator**: For servo motor and thermal printer
- **3.3V Regulator**: For ESP32, SD card, and sensors
- **Bulk Capacitor**: 1000-2200 µF near thermal printer
- **Power Connectors**: Appropriate connectors for power distribution

### Connectors
- Male headers - for ESP32-WROVER and ESP32-C3 SuperMini
- Female headers - for LEDs, capacitive sensor, and thermal printer

### Skull + Enclosure
- plastic skull with light-up eyes that you can take apart
- polymer clay
- per servo ear mounting hardware:
  - m2 brass threaded heat set insert
  - 2x silicone o-rings (6x1.9mm / 1 5/64" x 5/64")
  - flat brass m3 washer
  - m2x10 screw
- ?? DIAMETER ?? brass rod for jaw movement
- 

## Prototyping Power Setup

### Problem: USB-C Back-Feed Protection

When using the Freenove ESP32-WROVER breakout board with barrel jack power and USB-C programming cable simultaneously, the computer detects power back-feeding through USB-C and shuts down the connection for safety. This prevents programming via USB-C.

### Solution: Separate Power Rails with Common Ground

Use two separate power supplies with **shared ground** to power the ESP32 and peripherals independently:

#### Required Components
- **Freenove ESP32-WROVER Breakout Board** (with ESP32-WROVER module)
- **ESP32-C3 SuperMini Expansion Board** (used as passive breakout/power distribution)
- **USB-C cable** (for ESP32 programming/power)
- **USB power bank** or separate 5V supply (for peripherals)
- **Jumper wires** for ground connection

#### Wiring Configuration

```
Freenove Breakout Board:
  ├─ ESP32-WROVER (powered via USB-C)
  │  └─ GND ────────────┐
  │                     │
  └─ GND terminal ──────┼───> COMMON GROUND
                        │
ESP32-C3 Expansion Board: │
  (No module inserted - passive breakout)
  ├─ Pin 1 (5V) ←────── USB Power Bank 5V
  ├─ Pin 2 (GND) ───────┘ ────┐
  │                            │
                               └───> Common Ground (tied to Freenove GND)
```

#### Step-by-Step Setup

1. **Freenove Breakout Board:**
   - Insert ESP32-WROVER module
   - Connect USB-C cable to ESP32 module for programming/power
   - **Do NOT connect barrel jack power** (prevents back-feed)

2. **ESP32-C3 Expansion Board:**
   - **Do NOT insert ESP32-C3 module** (used as passive breakout)
   - Connect USB power bank to Pin 1 (5V) and Pin 2 (GND)
   - This board now functions as a 5V power distribution hub

3. **MicroSD Card:**
   - Insert card into the built-in slot on the underside of the Freenove board (label toward PCB)
   - No external SPI wiring required — SDMMC traces are internal to the board
   - Keep the card inserted before powering on; the firmware mounts `/sdcard` during boot

4. **Common Ground Connection:**
   - Connect ESP32-C3 expansion board Pin 2 (GND) to Freenove breakout board GND terminal
   - This is **critical** - both power sources must share the same ground reference for digital peripherals (SDMMC, UART, etc.)

#### Why This Works

- **Separate Power Sources:** ESP32 gets power from USB-C; peripherals get power from USB power bank
- **No Back-Feed:** USB-C only powers ESP32, not the breakout board's 5V rail
- **Common Ground:** Both supplies share ground reference, enabling reliable SPI/I2C/UART communication
- **Passive Expansion Board:** ESP32-C3 expansion board without module is just a passive breakout, perfect for power distribution

#### Benefits

- ✅ No USB-C back-feed issues
- ✅ ESP32 programmable via USB-C
- ✅ Peripherals get adequate 5V power (servo/printer draw remain isolated)
- ✅ Uses existing hardware (no diodes or modifications needed)
- ✅ Full functionality: programming, debugging, and operation all work

#### Notes

- **Ground is NOT specific to a power source** - multiple DC power supplies can and should share a common ground in the same circuit
- This is standard practice for multi-supply systems (USB peripherals, Arduino shields, etc.)
- For production perfboard, add a Schottky diode (1N5817/1N5819) between external power and 5V rail to allow simultaneous USB-C and barrel jack power

## TEMP ROUGH OUTLINE
- get plastic skull, ideally one that can be opened up with eyes that already light up)
  - otherwise you need to cut it open and deal with the mess, figuring out how to close it, and the lack of mounting points inside will be a pain in the butt
- servo mounting hardware (one set for each ear): 2x silicone o-rings (6x1.9mm / 1 5/64" x 5/64"), flat brass m3 washer, m2x10 screw, brass threaded m2 heat set insert
    - the heat set insert is pushed into the polyer clay mount with a soldering iron
    - once cool, attach the servo, mounted in this order: o-ring, servo ear, o-ring, washer, screw 
