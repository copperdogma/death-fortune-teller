# LED Cluster Wiring Diagram

## Single 4-Wire Cable Solution for All Skull LEDs

This shows exactly how to wire all 3 LEDs with a single connector, keeping the servo completely separate.

## 📌 Connection Overview

```
ESP32-WROVER                     SKULL
┌────────────┐                   ┌─────────────────┐
│            │                   │                 │
│ Pin 12 ────┼──[GREEN]──────────┼──→ Left Eye    │
│ Pin 13 ────┼──[GREEN]──────────┼──→ Right Eye   │
│ Pin 14 ────┼──[GREEN]──────────┼──→ Mouth       │
│ GND    ────┼──[BLACK]──────────┼──→ Common GND  │
│            │                   │                 │
│ Pin 15 ────┼═══[3-PIN SERVO]═══┼──→ Jaw Motor   │
│            │   (separate)       │                 │
└────────────┘                   └─────────────────┘

LED Cable: Single 4-wire ribbon or bundle
Servo: Standard 3-pin servo connector (separate)
```

## 🔌 LED Connector Options

### Option A: JST-XH 4-Pin Connector (Recommended)
```
Female on Board Side:           Male on LED Cable:
┌──────────────┐                ┌──────────────┐
│ ○ ○ ○ ○      │                │ ● ● ● ●      │
│ 1 2 3 4      │◄───────────────┤ 1 2 3 4      │
└──────────────┘                └──────────────┘
 │ │ │ └─ GND                    │ │ │ └─ GND (Black)
 │ │ └─── Mouth (GPIO12)         │ │ └─── Mouth (Green)
 │ └───── R.Eye (GPIO14)         │ └───── R.Eye (Green)
 └─────── L.Eye (GPIO27)         └─────── L.Eye (Green)

JST-XH: 2.54mm pitch, locking, reliable
Perfect for permanent installation
```

### Option B: Dupont 4-Pin Header
```
Simple 0.1" Header:
┌─┬─┬─┬─┐
│●│●│●│●│ Female header on perfboard
└─┴─┴─┴─┘
 │ │ │ └─ GND
 │ │ └─── GPIO12 (Mouth)
 │ └───── GPIO14 (Right Eye)
 └─────── GPIO27 (Left Eye)

Dupont jumper wires with housing
Easy to modify/debug
```

### Option C: Terminal Block
```
Screw Terminal (4-position):
┌─────────────────────┐
│ ▄ ▄ ▄ ▄             │
│ █ █ █ █             │
│ ▀ ▀ ▀ ▀             │
└─────────────────────┘
  1 2 3 4
  │ │ │ └─ GND
  │ │ └─── Mouth
  │ └───── Right Eye
  └─────── Left Eye

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
 3 │     From ESP32 Pins 12-14:            │
 4 │     ○ ○ ○   [CONNECTOR]               │
 5 │     │ │ │                             │
 6 │    [R][R][R]  ← 100Ω resistors        │
 7 │     │ │ │                             │
 8 │     ○ ○ ○ ○  [TO SKULL]              │
 9 │     L R M G                           │
   └───────────────────────────────────────┘

Actual connections:
- Pin 12 → R1 (100Ω) → Connector Pin 1 (Left Eye)
- Pin 13 → R2 (100Ω) → Connector Pin 2 (Right Eye)
- Pin 14 → R3 (100Ω) → Connector Pin 3 (Mouth)
- GND    →           → Connector Pin 4 (Common)
```

## 💡 LED Wiring at Skull End

### Inside the Skull
```
4-Wire Cable Arrives:
        ┌──────────────┐
        │   CABLE IN   │
        │ ┌─┬─┬─┬─┐   │
        │ │1│2│3│4│   │
        └─┴─┴─┴─┴─────┘
          │ │ │ └──────────┐ (GND)
          │ │ └────────┐   │
          │ └──────┐   │   │
          │        │   │   │
          ▼        ▼   ▼   ▼
       ┌────┐  ┌────┐ ┌────┐
       │LED1│  │LED2│ │LED3│
       │ L  │  │ R  │ │ M  │
       └──┬─┘  └──┬─┘ └──┬─┘
          │       │       │
          └───────┴───────┘
               (Common GND)

LEDs:
- LED1: Left Eye (red LED)
- LED2: Right Eye (red LED)  
- LED3: Mouth (red or white LED)
```

## 🎨 Cable Management

### Clean Routing
```
ESP32 Board Area:
┌────────────────────────┐
│ ESP32-WROVER           │
│                        │
│ Pins 12-14 ──┐         │
│              ▼         │
│         [CONNECTOR]    │
│              │         │
└──────────────┼─────────┘
               │
          [4-WIRE CABLE]
          Single bundle
               │
               ▼
         ┌──────────┐
         │  SKULL   │
         │  ● ● ●   │ (3 LEDs)
         └──────────┘

Servo cable runs separately:
Pin 15 ══════[SERVO WIRE]═════> Jaw Motor
```

## 📊 Comparison: Old vs New

### Old Method (Mixed Grouping)
```
Wires needed:
- Left Eye:  1 wire from pin 32
- Right Eye: 1 wire from pin 33  
- Mouth:     1 wire from pin 25
- Grounds:   3 separate returns
- Total:     6 wires to skull
- Plus:      Messy routing
```

### New Method (Clustered)
```
Wires needed:
- Single 4-wire cable (or ribbon)
- Pins 12-14 are adjacent
- One ground return
- Total:     1 cable to skull!
- Plus:      Clean, professional
```

## ✅ Assembly Checklist

### At the Board
- [ ] Solder 4-pin connector at pins 12-14 + GND
- [ ] Add 100Ω resistors inline
- [ ] Test each LED channel with multimeter

### Cable Preparation  
- [ ] Cut 4-wire cable to length (board to skull)
- [ ] Strip and tin wire ends
- [ ] Crimp/solder connector on board end
- [ ] Leave skull end bare for now

### At the Skull
- [ ] Route cable through skull opening
- [ ] Solder Pin 1 to left eye LED anode
- [ ] Solder Pin 2 to right eye LED anode
- [ ] Solder Pin 3 to mouth LED anode
- [ ] Connect all LED cathodes to Pin 4 (GND)
- [ ] Hot glue LEDs in position

### Testing
- [ ] Test with example code
- [ ] Verify all LEDs respond
- [ ] Check no shorts or opens
- [ ] Confirm servo still works independently

## 💀 Final Result

**ONE** neat cable runs to your skull carrying all LED signals, while the servo maintains its standard independent connection. Professional, clean, and easy to troubleshoot!