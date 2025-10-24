# Hardware Documentation

This document catalogs the bill of materials and pin assignments for the Death Fortune Teller project, combining features from TwoSkulls and three proof-of-concept modules.

## Bill of Materials

### Core Microcontrollers
- **ESP32-WROVER Module**: Main controller with Wi-Fi/Bluetooth
- **ESP32-C3 SuperMini**: Matter controller with Wi-Fi/Bluetooth 5 (LE)

### Audio System
- **Bluetooth Speaker**: Hidden speaker for A2DP audio output
- **SD Card Module**: FZ0829-micro-sd-card-module for audio storage
- **SD Card**: MicroSD card with audio files and configuration

### Visual System
- **Eye LEDs**: Red LEDs (x2) with 100Ω current limiting resistor
- **Mouth LED**: Additional LED for mouth illumination

### Motion System
- **Servo Motor**: Power HD HD-1160A (4.8v-6v, 3.0kg.cm torque, 0.8A max)

### Sensor System
- **Touch Sensor**: ESP32 built-in touch sensor
- **Capacitive Sensor**: External capacitive sensor
- **Touch Electrode**: Conductive material for finger contact
- **Optional Resistor**: 1MΩ for touch sensitivity adjustment

### Printing System
- **Thermal Printer**: CSN-A1X or compatible 58mm thermal printer
- **Printer Power**: 5V ≥2A supply (or 9V with regulator)

### Power System
- **5V Regulator**: For servo motor and thermal printer
- **3.3V Regulator**: For ESP32, SD card, and sensors
- **Bulk Capacitor**: 1000-2200 µF near thermal printer
- **Power Connectors**: Appropriate connectors for power distribution

## Current Pin Assignments (CONFLICTED)

### LED Control
- **GPIO 32**: Left Eye LED (PWM Channel 0)
- **GPIO 33**: Right Eye LED (PWM Channel 1)
- **GPIO 2**: Mouth LED ⚠️ **CONFLICT**

### Servo Control
- **GPIO 15**: Servo control pin (PWM for jaw movement)

### SD Card Interface
- **GPIO 5**: SD Card CS (Chip Select)
- **GPIO 18**: SD Card SCK ⚠️ **CONFLICT**
- **GPIO 23**: SD Card MOSI (Master Out Slave In)
- **GPIO 19**: SD Card MISO ⚠️ **CONFLICT**

### UART Communication
- **GPIO 20**: UART RX ⚠️ **CONFLICT**
- **GPIO 21**: UART TX ⚠️ **CONFLICT**
- **GPIO 1**: Matter trigger pin (interrupt-driven)

### Sensors
- **GPIO 4**: Capacitive finger sensor pin
- **GPIO 2**: Touch sensor pin ⚠️ **CONFLICT**

### Thermal Printer
- **GPIO 18**: ESP32 TX → Printer RXD ⚠️ **CONFLICT**
- **GPIO 19**: ESP32 RX ← Printer TXD ⚠️ **CONFLICT**

### ESP32-C3 SuperMini (Matter Node)
- **GPIO 20**: UART RX (from ESP32-WROVER TX)
- **GPIO 21**: UART TX (to ESP32-WROVER RX)
- **GPIO 8**: Built-in LED (inverted logic)
- **GPIO 9**: BOOT button (factory reset)

## ⚠️ CRITICAL PIN CONFLICTS

The following pins have multiple conflicting assignments:

1. **GPIO 2**: Mouth LED vs Touch Sensor vs Built-in LED
2. **GPIO 18**: SD Card SCK vs UART RX vs Thermal Printer TX
3. **GPIO 19**: SD Card MISO vs Thermal Printer RX
4. **GPIO 20**: UART RX (multiple UART functions)
5. **GPIO 21**: UART TX (multiple UART functions)

## Hardware Specifications

### Servo Motor (Power HD HD-1160A)
- **Voltage**: 4.8v-6v
- **Stall Torque**: 3.0kg.cm
- **Max Current**: 0.8A
- **Speed**: 0.12s/60deg = 500 deg/s max speed
- **Dimensions**: 28 x 13.2 x 29.6 mm
- **Control**: PWM (Pulse Width Modulation)

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

| Pin Number | Pin Name | Functionality |
|------------|----------|---------------|
| 1          | GND      | Ground        |
| 2          | 3V3      | 3.3V Power Supply |
| 3          | EN       | Enable (Active High) |
| 4          | IO36     | ADC1_CH0, VP (Analog Input) |
| 5          | IO39     | ADC1_CH3, VN (Analog Input) |
| 6          | IO34     | ADC1_CH6 (Analog Input) |
| 7          | IO35     | ADC1_CH7 (Analog Input) |
| 8          | IO32     | ADC1_CH4, Touch9 |
| 9          | IO33     | ADC1_CH5, Touch8 |
| 10         | IO25     | DAC1, ADC2_CH8 |
| 11         | IO26     | DAC2, ADC2_CH9 |
| 12         | IO27     | ADC2_CH7, Touch7 |
| 13         | IO14     | ADC2_CH6, Touch6, HSPI_CLK |
| 14         | IO12     | ADC2_CH5, Touch5, HSPI_MISO |
| 15         | IO13     | ADC2_CH4, Touch4, HSPI_MOSI |
| 16         | IO15     | ADC2_CH3, Touch3, HSPI_CS |
| 17         | IO2      | ADC2_CH2, Touch2 |
| 18         | IO0      | ADC2_CH1, Touch1 |
| 19         | IO4      | ADC2_CH0, Touch0 |
| 20         | IO16     | UART2_RX |
| 21         | IO17     | UART2_TX |
| 22         | IO5      | GPIO5 |
| 23         | IO18     | VSPI_CLK |
| 24         | IO19     | VSPI_MISO |
| 25         | IO21     | I2C SDA |
| 26         | IO22     | I2C SCL |
| 27         | IO23     | VSPI_MOSI |

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
| 15         | IO20     | Digital Only  |
| 16         | IO21     | Digital Only  |

## Wiring Connections

### Power Distribution
1. **5V Rail**: Connect to servo motor, thermal printer, and voltage regulator input
2. **3.3V Rail**: Connect to ESP32, SD card module, and sensors
3. **Common Ground**: All components share a common ground reference

### Signal Connections
1. **LEDs**: Connect eye LEDs to GPIO 32/33 with 100Ω current limiting resistor
2. **Servo**: Connect control wire to GPIO 15, power to 5V rail, ground to common ground
3. **SD Card**: Connect SPI pins as specified, power to 3.3V
4. **UART**: Connect RX/TX pins between ESP32-WROVER and ESP32-C3 SuperMini
5. **Thermal Printer**: Connect UART pins to ESP32-WROVER
6. **Touch Sensor**: Connect electrode to GPIO 2 with optional 1MΩ resistor

### Important Notes
- **Power Requirements**: Ensure 5V rail can supply ≥3A for thermal printer operation
- **Bulk Capacitor**: Place 1000-2200 µF capacitor near thermal printer for power stability
- **Ground Plane**: Use a common ground plane for all components to avoid ground loops
- **Wire Gauge**: Use appropriate wire gauge for power connections (thicker for 5V rail)

## ⚠️ CRITICAL ISSUES REQUIRING RESOLUTION

### Pin Assignment Conflicts
The combination of TwoSkulls + three POCs creates **unresolvable pin conflicts** that must be addressed before hardware assembly:

1. **GPIO 2**: Mouth LED vs Touch Sensor vs Built-in LED
2. **GPIO 18**: SD Card SCK vs UART RX vs Thermal Printer TX
3. **GPIO 19**: SD Card MISO vs Thermal Printer RX
4. **GPIO 20/21**: Multiple UART functions (Matter controller vs ESP32-C3 vs Thermal Printer)

### Required Actions
1. **Re-plan all pin assignments** to eliminate conflicts
2. **Consider alternative interfaces** (e.g., software SPI for SD card)
3. **Evaluate pin multiplexing** where possible
4. **Document final pin assignments** before hardware assembly

### Hardware Integration Challenges
- **Power Requirements**: 5V rail must supply ≥3A for thermal printer + servo
- **UART Interfaces**: Multiple UART devices require careful pin management
- **SPI Interface**: SD card requires dedicated SPI pins
- **Touch Sensing**: Requires dedicated pin with proper grounding

### Next Steps
1. **Pin Reassignment**: Create new pin assignment plan
2. **Interface Optimization**: Minimize pin usage through multiplexing
3. **Hardware Validation**: Test each peripheral independently
4. **Integration Testing**: Verify all systems work together

**⚠️ DO NOT PROCEED WITH HARDWARE ASSEMBLY** until pin conflicts are resolved.

## Troubleshooting

- **SD Card Issues**: Verify 3.3V power supply and SPI connections
- **Servo Problems**: Check 5V power supply and PWM signal integrity
- **UART Communication**: Verify baud rate and frame format compatibility
- **Power Dips**: Ensure adequate bulk capacitance near high-current components
- **Touch Sensor**: Check electrode connection and grounding
- **Thermal Printer**: Verify 5V power supply and UART connections