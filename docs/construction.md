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
- **SD Card Module**: FZ0829-micro-sd-card-module for audio storage
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
- Female headers - for LEDs, capacitive sensor, SD card module, and thermal printer

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


## TEMP ROUGH OUTLINE
- get plastic skull, ideally one that can be opened up with eyes that already light up)
  - otherwise you need to cut it open and deal with the mess, figuring out how to close it, and the lack of mounting points inside will be a pain in the butt
- servo mounting hardware (one set for each ear): 2x silicone o-rings (6x1.9mm / 1 5/64" x 5/64"), flat brass m3 washer, m2x10 screw, brass threaded m2 heat set insert
    - the heat set insert is pushed into the polyer clay mount with a soldering iron
    - once cool, attach the servo, mounted in this order: o-ring, servo ear, o-ring, washer, screw 