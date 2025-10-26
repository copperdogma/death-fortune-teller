# Hardware Documentation

This document catalogs the bill of materials and pin assignments for the Death Fortune Teller project, combining features from TwoSkulls and three proof-of-concept modules.

** NOTE: **DIODE** We'll need a diode to keep power from flowing backwards, When the ESP32 WROVER or SuperMini is plugged in to reprogram or monitor, we don't want it to backdflow and try to power the rest of the system.

**Important Note**: All pin assignments in code use GPIO numbers, not physical board pin numbers. The FREENOVE ESP32-WROVER board has physical pins numbered 1-38, but code must reference the GPIO numbers.

## Table of Contents

- [Bill of Materials](#bill-of-materials)
- [Current Pin Assignments](#current-pin-assignments)
- [Pin Conflicts Analysis](#pin-conflicts-analysis)
- [Hardware Specifications](#hardware-specifications)
- [Power Requirements](#power-requirements)
- [ESP32-WROVER Pinout Reference](#esp32-wrover-pinout-reference)
- [ESP32-C3 SuperMini Pinout Reference](#esp32-c3-supermini-pinout-reference)
- [Wiring Connections](#wiring-connections)
- [Recommended Solution](#recommended-solution)
- [Implementation Notes](#implementation-notes)
- [Troubleshooting](#troubleshooting)
- [Revision History](#revision-history)

## Bill of Materials

### Core Microcontrollers
- **FREENOVE ESP32-WROVER Development Board**: Main controller with Wi-Fi/Bluetooth and PSRAM
- **ESP32-C3 SuperMini**: Matter controller with Wi-Fi/Bluetooth 5 (LE)

### Audio System
- **Bluetooth Speaker**: Hidden speaker for A2DP audio output
- **SD Card Module**: FZ0829-micro-sd-card-module for audio storage
- **SD Card**: MicroSD card with audio files and configuration

### Visual System
- **Eye LEDs**: Pink LEDs (x2) with 100Ω current limiting resistor
- **Mouth LED**: Pink LED for mouth illumination

### Motion System
- **Servo Motor**: Hitec HS-125MG (4.8v-6v, 3.0-3.5kg.cm torque, 1.2A stall current)

### Sensor System
- ~10x10cm copper foil for capacitive sensor
- coax (shielded) cable for capacitive sensor

### Printing System
- **Thermal Printer**: CSN-A1X or compatible 58mm thermal printer
- **Printer Power**: 5V ≥2A supply (or 9V with regulator)

### Power System
- **5V Regulator**: For servo motor, thermal printer, and SD card module
- **3.3V Regulator**: For ESP32 and sensors
- **Bulk Capacitor**: 1000-2200 µF near thermal printer
- **Power Connectors**: Appropriate connectors for power distribution

## Current Pin Assignments

### LED Control
- **GPIO 32**: Left Eye LED (PWM Channel 0)
- **GPIO 33**: Right Eye LED (PWM Channel 1)
- **GPIO 2**: Mouth LED ⚠️ **POTENTIAL CONFLICT**

### Servo Control
- **GPIO 15**: Servo control pin (PWM for jaw movement)

### SD Card Interface (SPI)
- **GPIO 5**: SD Card CS (Chip Select)
- **GPIO 18**: SD Card SCK (Serial Clock) ⚠️ **POTENTIAL CONFLICT**
- **GPIO 23**: SD Card MOSI (Master Out Slave In)
- **GPIO 19**: SD Card MISO (Master In Slave Out) ⚠️ **POTENTIAL CONFLICT**

### UART Communication (Initial Assignment)
- **GPIO 16**: UART RX (from ESP32-C3) ⚠️ **POTENTIAL CONFLICT**
- **GPIO 17**: UART TX (to ESP32-C3) ⚠️ **POTENTIAL CONFLICT**
- **GPIO 1**: Matter trigger pin (interrupt-driven)

### Sensors
- **GPIO 4**: Capacitive finger sensor pin
- **GPIO 2**: Touch sensor pin ⚠️ **POTENTIAL CONFLICT**

### Thermal Printer (Initial Assignment)
- **GPIO 18**: ESP32 TX → Printer RXD ⚠️ **POTENTIAL CONFLICT**
- **GPIO 19**: ESP32 RX ← Printer TXD ⚠️ **POTENTIAL CONFLICT**

### ESP32-C3 SuperMini (Matter Node)
- **GPIO 20**: UART RX (from ESP32-WROVER TX)
- **GPIO 21**: UART TX (to ESP32-WROVER RX)
- **GPIO 8**: Built-in LED (inverted logic)
- **GPIO 9**: BOOT button (factory reset)

## Pin Conflicts Analysis

The following pins have multiple conflicting assignments that need resolution:

1. **GPIO 2**: Mouth LED vs Touch Sensor (also connected to on-board LED on some ESP32 boards)
2. **GPIO 18**: SD Card SCK vs Thermal Printer TX
3. **GPIO 19**: SD Card MISO vs Thermal Printer RX
4. **GPIO 16/17**: Currently assigned to Matter UART, but could conflict if multiple UART devices share pins

**Resolution Strategy**: Use ESP32's multiple hardware UART controllers to eliminate conflicts (see Recommended Solution section).

## Hardware Specifications

### Servo Motor (Hitec HS-125MG)
- **Voltage**: 4.8v-6v
- **Stall Torque**: 3.0kg.cm (4.8V) / 3.5kg.cm (6V)
- **Stall Current**: 1.2A
- **Speed**: 0.17s/60deg (4.8V) / 0.13s/60deg (6V)
- **Dimensions**: 30.0 x 10.0 x 34.0 mm (L x W x H)
- **Weight**: 24g
- **Control**: PWM (Pulse Width Modulation)
- **Gear Type**: Metal gears with dual ball bearings
- **Output Shaft**: Micro 25 spline

### Touch Sensor
- **Sensitivity**: 0.2% change threshold (configurable)
- **Debounce**: 30ms (configurable)
- **Baseline**: Auto-calibrating with drift compensation
- **Detection**: Normalized increase detection for ESP32-S3

### Thermal Printer
- **Baud Rate**: 9600 8N1
- **Max Width**: 384 dots
- **Print Mode**: Raster bitmap (1bpp)
- **Commands**: ESC/POS compatible

### UART Protocol (Matter Communication)
- **Baud Rate**: 115200
- **Data Format**: 8N1 (8 data bits, no parity, 1 stop bit)
- **Frame Format**: 0xA5 [LEN][CMD][PAYLOAD...][CRC8]
- **CRC**: CRC-8 polynomial 0x31, initial value 0x00
- **Commands**: HELLO(0x01), SET_MODE(0x02), TRIGGER(0x03), PING(0x04)
- **Status**: PAIRED(0x10), UNPAIRED(0x11)
- **Responses**: ACK(0x80), ERR(0x81), BUSY(0x82), DONE(0x83)

## Power Requirements

- **5V Rail**: ≥3A for thermal printer + servo + LEDs
- **3.3V Rail**: For ESP32, SD card, and sensors
- **Bulk Capacitor**: 1000-2200 µF near thermal printer for power stability

## ESP32-WROVER Pinout Reference

**Note**: This table shows the FREENOVE ESP32-WROVER development board pinout. Column 1 is the physical pin number on the board, Column 2 is the GPIO name used in code.

| Physical Pin | GPIO Name | Functionality | Notes |
|--------------|-----------|---------------|--------|
| 1            | GND       | Ground        | |
| 2            | 3V3       | 3.3V Power Supply | |
| 3            | EN        | Enable (Active High) | |
| 4            | IO36      | ADC1_CH0, VP (Input Only) | Analog Input Only |
| 5            | IO39      | ADC1_CH3, VN (Input Only) | Analog Input Only |
| 6            | IO34      | ADC1_CH6 (Input Only) | Analog Input Only |
| 7            | IO35      | ADC1_CH7 (Input Only) | Analog Input Only |
| 8            | IO32      | ADC1_CH4, Touch9 | |
| 9            | IO33      | ADC1_CH5, Touch8 | |
| 10           | IO25      | DAC1, ADC2_CH8 | |
| 11           | IO26      | DAC2, ADC2_CH9 | |
| 12           | IO27      | ADC2_CH7, Touch7 | |
| 13           | IO14      | ADC2_CH6, Touch6, HSPI_CLK | |
| 14           | IO12      | ADC2_CH5, Touch5, HSPI_MISO | Boot Strapping Pin |
| 15           | IO13      | ADC2_CH4, Touch4, HSPI_MOSI | |
| 16           | IO15      | ADC2_CH3, Touch3, HSPI_CS | Boot Strapping Pin |
| 17           | IO2       | ADC2_CH2, Touch2, On-board LED | Boot Strapping Pin |
| 18           | IO0       | ADC2_CH1, Touch1, Boot Button | Boot Strapping Pin |
| 19           | IO4       | ADC2_CH0, Touch0 | |
| 20           | IO16      | UART2_RX | |
| 21           | IO17      | UART2_TX | |
| 22           | IO5       | VSPI_CS | |
| 23           | IO18      | VSPI_CLK | |
| 24           | IO19      | VSPI_MISO | |
| 25           | IO21      | I2C SDA | |
| 26           | IO22      | I2C SCL | |
| 27           | IO23      | VSPI_MOSI | |

**Important GPIO Notes**:
- GPIO 34-39 are input-only pins
- GPIO 0, 2, 12, 15 are strapping pins (affect boot mode)
- ESP32 does NOT have GPIO 20, 21, 24, 28-31 (these numbers are skipped)

## ESP32-C3 SuperMini Pinout Reference

| Pin Number | Pin Name | Functionality |
|------------|----------|---------------|
| 1          | 5V       | External or USB Power |
| 2          | G        | Ground        |
| 3          | 3.3V     | 3.3V Power Supply |
| 4          | IO0      | Digital and Analog (ADC1) |
| 5          | IO1      | Digital and Analog (ADC1) |
| 6          | IO2      | Digital and Analog (ADC1), Strapping Pin |
| 7          | IO3      | Digital and Analog (ADC1) |
| 8          | IO4      | Digital and Analog (ADC1) |
| 9          | IO5      | Digital and Analog (ADC1) |
| 10         | IO6      | Digital Only  |
| 11         | IO7      | Digital Only  |
| 12         | IO8      | Digital Only, Connected to Blue LED (Inverted) |
| 13         | IO9      | Digital Only, Strapping Pin, Boot Button |
| 14         | IO10     | Digital Only  |
| 15         | IO20     | Digital Only, UART RX Default |
| 16         | IO21     | Digital Only, UART TX Default |

**Note**: ESP32-C3 DOES have GPIO20 and GPIO21 (unlike classic ESP32).

## Wiring Connections

### Power Distribution
1. **5V Rail**: Connect to servo motor, thermal printer, SD card module, and voltage regulator input
2. **3.3V Rail**: Connect to ESP32 and sensors
3. **Common Ground**: All components share a common ground reference

### Signal Connections
1. **LEDs**: Connect eye LEDs to GPIO 32/33 with 100Ω current limiting resistor
2. **Servo**: Connect control wire to GPIO 15, power to 5V rail, ground to common ground
3. **SD Card**: Connect SPI pins as specified, power to 5V
4. **UART**: Connect RX/TX pins between ESP32-WROVER and ESP32-C3 SuperMini
5. **Thermal Printer**: Connect UART pins to ESP32-WROVER
6. **Touch Sensor**: Connect electrode to GPIO 2 or GPIO 4 (choose one)

### Important Notes
- **Power Requirements**: Ensure 5V rail can supply ≥3A for thermal printer operation
- **Bulk Capacitor**: Place 1000-2200 µF capacitor near thermal printer for power stability
- **Ground Plane**: Use a common ground plane for all components to avoid ground loops
- **Wire Gauge**: Use appropriate wire gauge for power connections (AWG 20-22 for 5V rail)

## Recommended Solution

### Multiple Hardware UART Solution

The ESP32-WROVER has **3 hardware UART interfaces** (UART0, UART1, UART2), allowing each communication device to use its own dedicated UART controller, eliminating all pin conflicts.

#### Final Pin Assignment (Conflict-Free)

```cpp
// UART2: ESP32-C3 SuperMini (Matter Controller)
#define MATTER_UART_NUM     UART_NUM_2
#define MATTER_TX_PIN       17  // GPIO17 -> C3 GPIO20
#define MATTER_RX_PIN       16  // GPIO16 <- C3 GPIO21
#define MATTER_BAUD_RATE    115200

// UART1: Thermal Printer  
#define PRINTER_UART_NUM    UART_NUM_1
#define PRINTER_TX_PIN      4   // GPIO4 -> Printer RXD
#define PRINTER_RX_PIN      5   // GPIO5 <- Printer TXD  
#define PRINTER_BAUD_RATE   9600

// SPI: SD Card (Hardware VSPI)
#define SD_CS_PIN           5   // GPIO5 - Chip Select
#define SD_SCK_PIN          18  // GPIO18 - Serial Clock (VSPI default)
#define SD_MOSI_PIN         23  // GPIO23 - Master Out Slave In (VSPI default)
#define SD_MISO_PIN         19  // GPIO19 - Master In Slave Out (VSPI default)

// LEDs
#define LEFT_EYE_PIN        32  // GPIO32 - PWM Channel 0
#define RIGHT_EYE_PIN       33  // GPIO33 - PWM Channel 1  
#define MOUTH_LED_PIN       25  // GPIO25 - PWM Channel 2 (moved from GPIO2)

// Servo
#define SERVO_PIN           15  // GPIO15 - PWM for jaw movement

// Sensors
#define TOUCH_SENSOR_PIN    4   // GPIO4 - Capacitive touch (Touch0)
#define FINGER_SENSOR_PIN   27  // GPIO27 - External capacitive sensor (Touch7)

// Matter Trigger
#define MATTER_TRIGGER_PIN  1   // GPIO1 - Interrupt-driven
```

#### Hardware Wiring Diagram

```
ESP32-WROVER (FREENOVE) Connections:
┌─────────────────────────┐
│ ESP32-WROVER            │
│                         │
│ GPIO17 (Pin 21) ────────┼───→ ESP32-C3 GPIO20 (RX)
│ GPIO16 (Pin 20) ←───────┼─── ESP32-C3 GPIO21 (TX)
│                         │
│ GPIO4 (Pin 19) ─────────┼───→ Thermal Printer RXD
│ GPIO5 (Pin 22) ←────────┼─── Thermal Printer TXD
│                         │
│ GPIO18 (Pin 23) ────────┼───→ SD Card SCK
│ GPIO19 (Pin 24) ←───────┼─── SD Card MISO
│ GPIO23 (Pin 27) ────────┼───→ SD Card MOSI
│ GPIO5 (Pin 22) ─────────┼───→ SD Card CS
│                         │
│ GPIO32 (Pin 8) ─────────┼───→ Left Eye LED (via 100Ω)
│ GPIO33 (Pin 9) ─────────┼───→ Right Eye LED (via 100Ω)
│ GPIO25 (Pin 10) ────────┼───→ Mouth LED (via 100Ω)
│                         │
│ GPIO15 (Pin 16) ────────┼───→ Servo Control Signal
│                         │
│ GPIO4 (Pin 19) ─────────┼───→ Touch Electrode
│ GPIO27 (Pin 12) ────────┼───→ External Cap Sensor
│                         │
│ 3.3V ───────────────────┼───→ ESP32 + Sensors
│ 5V ─────────────────────┼───→ Servo + Printer + SD Card Power
│ GND ────────────────────┼───→ Common Ground
└─────────────────────────┘
```

#### Software Implementation Example

```cpp
// uart_manager.h
class UARTManager {
public:
    void begin() {
        // Initialize UART2 for Matter Controller
        uart_config_t matter_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
        };
        
        uart_param_config(UART_NUM_2, &matter_config);
        uart_set_pin(UART_NUM_2, MATTER_TX_PIN, MATTER_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(UART_NUM_2, 1024, 1024, 10, nullptr, 0);
        
        // Initialize UART1 for Thermal Printer
        uart_config_t printer_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
        };
        
        uart_param_config(UART_NUM_1, &printer_config);
        uart_set_pin(UART_NUM_1, PRINTER_TX_PIN, PRINTER_RX_PIN,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(UART_NUM_1, 256, 256, 0, nullptr, 0);
    }
    
    // Matter communication methods
    void sendMatterCommand(uint8_t cmd, uint8_t* payload, size_t len);
    bool hasMatterResponse();
    UARTCommand getMatterCommand();
    
    // Thermal printer methods
    void printFortune(const char* text);
    void printBitmap(const uint8_t* bitmap, uint16_t w, uint16_t h);
    bool isPrinterReady();
    
private:
    static constexpr int MATTER_TX_PIN = 17;
    static constexpr int MATTER_RX_PIN = 16;
    static constexpr int PRINTER_TX_PIN = 4;
    static constexpr int PRINTER_RX_PIN = 5;
};
```

### Optional Master Power Switch (Using Built-In Skull Switch)

The skull’s built-in battery compartment typically includes an **inline power switch**.  
Even if the batteries are unused, you can repurpose this switch as a **master power switch** for the 5 V rail.

#### Usage (switch-only; no batteries)

Wire the switch **in series on the 5 V input** feeding the perfboard:

1. **Isolate/ignore the battery contacts** so they don’t connect to anything.
2. Bring the two switch leads into the enclosure and terminate them on a **2-pin header** (e.g., `J_SW`).
3. Place the header **in series with the 5 V input** from your USB-C power:

USB-C 5V ──> [Skull Switch] ──> Perfboard 5V rail
USB-C GND ─────────────────────> Common GND

When the switch is **OFF**, it opens the 5 V line and the entire system powers down.

**Notes**
- Switch the **high side (5 V)**, not ground.
- Keep **common ground** shared between all supplies and boards.
- This integrates cleanly with the existing **Power Requirements** and **Power Distribution** sections.

#### Optional: add backup power from the built-in 3×AA pack (≈4.5 V)

If you want the skull to run when USB-C is unplugged, you can **OR** the 3×AA pack with a **Schottky diode** to prevent back-feeding:

                     +-------------------+
                     |   Skull Switch    |

USB-C 5V ────────────────+────o/ o───────────+───> Perfboard 5V rail
|                   |
3×AA + ──> Schottky ─────+                   |
(e.g. 1N5819)                      |
|
USB-C GND ───────────────────────────────────+───> Common GND
3×AA −  ─────────────────────────────────────+

**Details**
- Use a **Schottky diode** (e.g., 1N5817/1N5819, ≥2 A recommended for headroom) from **battery +** to the **switched 5 V rail** (banded end toward the rail).
- The switch still controls system power. With USB unplugged and the switch ON, the batteries supply the rail through the diode.
- Expect a **~0.2–0.4 V drop** across the diode; many loads tolerate this (printer/servo are on 5 V; ESP32 is on regulated 3.3 V).
- Maintain the **bulk capacitor** near peak loads (printer/servo) as already specified.

#### Safety / Integration checklist
- ✅ Do **not** route battery + directly to the 5 V rail without a diode (prevents back-feed into USB-C and programmers).  
- ✅ Keep **wire gauge** appropriate for 5 V rail current (AWG 20–22).  
- ✅ Place the master switch **before** any 5 V regulators and high-current loads (servo/printer).  
- ✅ Common ground between USB-C, batteries, and all modules.


## Implementation Notes

### Benefits of This Solution

1. **No Pin Conflicts**: Each UART device gets dedicated pins
2. **Hardware Efficiency**: Uses dedicated UART hardware, not software serial
3. **Concurrent Communication**: All devices can communicate simultaneously
4. **Protocol Isolation**: Each device maintains its own protocol and baud rate
5. **Error Isolation**: One device failure doesn't affect others

### Critical Considerations

1. **Boot Strapping Pins**: Avoid using GPIO 0, 2, 12, 15 for outputs that could interfere with boot
2. **Input-Only Pins**: GPIO 34-39 cannot be used for outputs
3. **UART0**: Reserved for USB/Serial debugging, not used for peripherals
4. **Power Sequencing**: Enable 5V rail before initializing thermal printer
5. **Ground Loops**: Ensure single-point grounding to avoid noise

### Power Budget

| Component | Current Draw | Voltage |
|-----------|-------------|---------|
| ESP32-WROVER | 250mA peak | 3.3V |
| ESP32-C3 | 100mA peak | 3.3V |
| Servo Motor (HS-125MG) | 1200mA stall | 5V |
| Thermal Printer | 2000mA peak | 5V |
| LEDs (3x) | 60mA total | 3.3V |
| SD Card Module | 200mA peak | 5V |
| **Total 5V Rail** | **≥3.1A required** | 5V |
| **Total 3.3V Rail** | **≥500mA required** | 3.3V |

## Troubleshooting

### Common Issues and Solutions

1. **SD Card Not Detected**
   - Verify 5V power supply
   - Check SPI connections (especially CS pin)
   - Ensure SD card is formatted as FAT32
   - Try lower SPI clock speed initially

2. **Servo Jittering (HS-125MG)**
   - Add bulk capacitor near servo power pins
   - Use separate 5V supply for servo if possible
   - Check PWM frequency (50Hz standard for servos)
   - Ensure adequate power supply (1.2A stall current)
   - Use Micro 25 spline servo horns/arms

3. **UART Communication Failures**
   - Verify baud rates match between devices
   - Check TX→RX and RX←TX connections
   - Ensure common ground between devices
   - Use pull-up resistors on UART lines if needed

4. **Touch Sensor Issues**
   - Calibrate baseline readings on startup
   - Shield sensor traces from noise sources
   - Adjust sensitivity thresholds
   - Add 1MΩ resistor between GPIO and electrode if needed

5. **Thermal Printer Problems**
   - Ensure adequate power supply (≥2A at 5V)
   - Check paper orientation and type
   - Verify ESC/POS command compatibility
   - Allow printer warm-up time

6. **Power Instability**
   - Add bulk capacitors (1000-2200µF) near high-current loads
   - Use thicker wires for power distribution (AWG 20-22)
   - Consider separate regulators for digital and motor power
   - Monitor voltage levels during peak loads

### Debug Commands

```cpp
// Test individual components
void runHardwareTest() {
    // Test LEDs
    testLEDs();
    
    // Test servo range
    testServo();
    
    // Test UART communication
    testMatterUART();
    testPrinterUART();
    
    // Test SD card
    testSDCard();
    
    // Test touch sensors
    testTouchSensors();
}
```

## Revision History

- **v1.0**: Initial documentation with pin conflicts
- **v2.0**: Corrected GPIO numbering (removed references to non-existent GPIO20/21 on ESP32)
- **v2.1**: Added FREENOVE board specifics and clarified physical pin vs GPIO numbering
- **Current (v3.0)**: Complete pin reassignment with conflict-free solution using multiple hardware UARTs