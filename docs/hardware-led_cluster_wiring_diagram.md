# LED Cluster Wiring Diagram

## Single 3-Wire Cable Solution for All Skull LEDs

This shows exactly how to wire both LEDs with a single connector, keeping the servo completely separate. It matches the final LED connector plan in docs/perfboard-assembly.md: pin order Eye, Mouth, GND.

## ⚡ Resistor Calculation

### LED Circuit Analysis
```
ESP32 GPIO Output: 3.3V
LED Forward Voltage (Vf): 3.0-3.2V (pink LED from BOJACK specs)
Desired LED Current: 20mA (safe operating current)

Resistor Value = (Vcc - Vf) / I
R = (3.3V - 3.1V) / 0.020A  (using 3.1V average)
R = 0.2V / 0.020A
R = 10Ω

Recommended: 100Ω (provides safety margin)
```

### Why 100Ω Instead of 10Ω?
- **Safety margin**: Prevents LED damage from voltage spikes
- **Longer LED life**: Lower current reduces heat and aging
- **ESP32 protection**: Reduces current draw on GPIO pins
- **Standard value**: 100Ω is a common resistor size
- **Brightness**: Still provides excellent visibility
- **Voltage tolerance**: Accounts for ESP32 voltage variations (3.0-3.6V)

**✅ Confirmed: 100Ω is the correct choice for this application**

## 📌 Connection Overview

```
ESP32-WROVER                     SKULL
┌────────────┐                   ┌─────────────────┐
│            │                   │                 │
│ GPIO32 ────┼──[PINK]───────────┼──→ Eye         │
│ GPIO33 ────┼──[PINK]───────────┼──→ Mouth       │
│ GND    ────┼──[BLACK]──────────┼──→ Common GND  │
│            │                   │                 │
│ GPIO23 ────┼═══[3-PIN SERVO]═══┼──→ Jaw Motor   │
│            │   (separate)       │                 │
└────────────┘                   └─────────────────┘

LED Cable: Single 3-wire ribbon or bundle (pink LEDs)
Servo: Standard 3-pin servo connector (separate)
```

## 🔌 LED Connector Options

### Option A: JST-XH 3-Pin Connector (Recommended)
```
Female on Board Side:           Male on LED Cable:
┌──────────────┐                ┌──────────────┐
│ ○ ○ ○        │                │ ● ● ●        │
│ 1 2 3        │◄───────────────┤ 1 2 3        │
└──────────────┘                └──────────────┘
 └───── Eye (GPIO32)             └───── Eye (Purple)
  └─── Mouth (GPIO33)            └─── Mouth (Purple)
   └─ GND                        └─ GND (Black)

JST-XH: 2.54mm pitch, locking, reliable
Perfect for permanent installation
```

### Option B: Dupont 3-Pin Header
```
Simple 0.1" Header:
┌─┬─┬─┐
│●│●│●│ Female header on perfboard
└─┴─┴─┘
 └───── GPIO32 (Eye)
  └─── GPIO33 (Mouth)
   └─ GND

Dupont jumper wires with housing
Easy to modify/debug
```

### Option C: Terminal Block
```
Screw Terminal (3-position):
┌─────────────────────┐
│ ▄ ▄ ▄               │
│ █ █ █               │
│ ▀ ▀ ▀               │
└─────────────────────┘
  1 2 3
  └───── Eye
   └─── Mouth
   └─ GND

No crimping needed
Field-serviceable
```

## 🛠️ LED Board Assembly

### Perfboard Layout for LED Driver
```
     A   B   C   D   E   F   G   H   I   J
   ┌───────────────────────────────────────┐
 1 │ ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● │ 3.3V Rail
 2 │ ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● │ GND Rail
   │                                       │
 3 │     From ESP32 GPIO32-33:             │
 4 │     ○ ○   [CONNECTOR]                 │
 5 │     │ │                               │
 6 │    [R][R]  ← 100Ω resistors           │
 7 │     │ │                               │
 8 │     ○ ○ ○  [TO SKULL]                 │
 9 │     E M G                             │
   └───────────────────────────────────────┘

Actual connections:
- Pin 32 → R1 (100Ω) → Connector Pin 1 (Eye)
- Pin 33 → R2 (100Ω) → Connector Pin 2 (Mouth)
- GND    →           → Connector Pin 3 (Common)
```

## 💡 LED Wiring at Skull End

### Inside the Skull
```
3-Wire Cable Arrives:
        ┌──────────────┐
        │   CABLE IN   │
        │ ┌─┬─┬─┐     │
        │ │1│2│3│     │
        └─┴─┴─┴───────┘
          │ │ └──────────┐ (GND)
          │ └────────┐   │
          │          │   │
          ▼          ▼   ▼
       ┌────┐     ┌────┐
       │LED1│     │LED2│
       │ E  │     │ M  │
       └──┬─┘     └──┬─┘
          │          │
          └──────────┘
               (Common GND)

LEDs:
- LED1: Eye (pink LED)
- LED2: Mouth (pink LED)
```

## 🎨 Cable Management

### Clean Routing
```
ESP32 Board Area:
┌────────────────────────┐
│ ESP32-WROVER           │
│                        │
│ GPIO32-33 ───┐         │
│              ▼         │
│         [CONNECTOR]    │
│              │         │
└──────────────┼─────────┘
               │
          [3-WIRE CABLE]
          Single bundle
               │
               ▼
         ┌──────────┐
         │  SKULL   │
         │  ● ●     │ (2 LEDs)
         └──────────┘

Servo cable runs separately:
GPIO23 ══════[SERVO WIRE]═════> Jaw Motor
```

## 📊 Comparison: Old vs New

### Old Method (Mixed Grouping)
```
Wires needed:
- Eye:       1 wire from pin 32
- Mouth:     1 wire from pin 33  
- Grounds:   2 separate returns
- Total:     4 wires to skull
- Plus:      Messy routing
```

### New Method (Clustered)
```
Wires needed:
- Single 3-wire cable (or ribbon)
- Pins 32-33 are adjacent
- One ground return
- Total:     1 cable to skull!
- Plus:      Clean, professional
```

## ✅ Assembly Checklist

### At the Board
- [ ] Solder 3-pin connector at pins 32-33 + GND
- [ ] Add 100Ω resistors inline
- [ ] Test each LED channel with multimeter

### Cable Preparation  
- [ ] Cut 3-wire cable to length (board to skull)
- [ ] Strip and tin wire ends
- [ ] Crimp/solder connector on board end
- [ ] Leave skull end bare for now

### At the Skull
- [ ] Route cable through skull opening
- [ ] Solder Pin 1 to eye LED anode
- [ ] Solder Pin 2 to mouth LED anode
- [ ] Connect both LED cathodes to Pin 3 (GND)
- [ ] Hot glue LEDs in position

### Testing
- [ ] Test with example code
- [ ] Verify all LEDs respond
- [ ] Check no shorts or opens
- [ ] Confirm servo still works independently

## 💀 Final Result

**ONE** neat 3-wire cable runs to your skull carrying both LED signals, while the servo maintains its standard independent connection. Professional, clean, and easy to troubleshoot!
