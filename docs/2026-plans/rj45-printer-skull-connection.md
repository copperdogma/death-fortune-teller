# RJ45 Printer Skull Connection

20251101: Created via GPT5 convo: https://chatgpt.com/g/g-p-68992f0599d081918441e6fcb4fac8a9-electronics/c/69062aa6-d9a4-8331-a0c9-08cebc84425c

NOTE: Do this. Simple, risk-free, and very very useful.

NOTE: The printer uses TWO different JST connectors: JST-XH (2.52mm pitch) which I have and, according to the manual, JST-PH (2.0mm pitch which I'm ordering now))

## To Do
- [ ] Test existing blue/gray cables below to see if all pins are straight though or if some are crossed

# Bill of Materials That We Have
- blue 6-8' male-male ethernet cable
- 3' gray male-female ethernet cable



## AI Writeup of Convo


# RJ-45 Cabling & Connector Plan

**Project:** *Death Animatronic Skeleton – Thermal Printer Link*
**Rev:** 1.1
**Date:** Nov 2025

---

## 🧩 Overview

This system links the thermal printer (mounted in the 50 lb typewriter prop) to the electronics perfboard inside the skull using standard **Cat5e RJ-45 cabling**.
It provides modular separation between the heavy printer assembly and the costumed skeleton, allowing easy transport, setup, and service.

---

## ⚙️ Connection Layout

```
[ Thermal Printer ]
   │
   ├─ 2× JST (printer interface)
   │
   ▼
Printer Cable – 3 ft  →  Male RJ-45
   │
   ▼
[ Male ↔ Female Ethernet Cable (3 ft) ]
   │
   ▼
[ Skull Panel Port – Female RJ-45 w/ 6″ Male Pigtail ]
   │
   ▼
[ RJ-45 Breakout Board on Perfboard ]
   │
   └─ 5 V / GND / TX / RX to local circuitry
```

---

## 🧰 Wiring Scheme (Cat5e, T568B Colors)

| Function                      | Pair | Wire Colors                    | Notes                                          |
| ----------------------------- | ---- | ------------------------------ | ---------------------------------------------- |
| **+5 V**                      | 1, 2 | Blue / W-Blue, Green / W-Green | Two full pairs in parallel for power           |
| **GND**                       | 1, 2 | Blue mates to W-Blue, etc.     | Each +5 V wire’s twisted partner is its return |
| **TX / RX**                   | 3    | Orange / W-Orange              | Keep as one pair for data                      |
| *(optional extra power pair)* | 4    | Brown / W-Brown                | Add if voltage drop > 0.2 V                    |

> Always pair each +5 V line with its GND partner to minimize EMI and loop area.

---

## ⚡ Electrical Notes

* 6 ft total cable run, 5 V @ 2 A max.
* 24 AWG Cat5e ≈ 0.026 Ω / ft / conductor.
* Two pairs → ≈ 0.16 V drop @ 2 A.
* Three pairs → ≈ 0.10 V drop @ 2 A.
* Add **1000–2200 µF low-ESR capacitor** at printer between 5 V and GND.
* Verify ≥ 4.75 V at printer under full load.
* Optionally set source to 5.1–5.2 V if safe for printer.

---

## 🪛 Mechanical Notes

* Use **stranded Cat5e patch cable** (not solid core).
* Use **pass-through RJ-45 plugs** for reliable crimps.
* Add **strain relief** or zip-tie anchors inside skull and typewriter.
* Label all endpoints: *Printer → Extension → Skull → Perfboard.*
* Panel jack should mount flush; add a rubber dust cap if exposed.

---

## 📦 Bill of Materials (BOM)

| Qty  | Component                                            | Description / Notes                              | Example Source      |
| ---- | ---------------------------------------------------- | ------------------------------------------------ | ------------------- |
| 1    | **Panel-mount RJ-45 Female Jack w/ 6″ Male Pigtail** | Skull external port                              | AliExpress / Amazon |
| 1    | **RJ-45 Breakout Board**                             | Perfboard interface for power + data             | Amazon / SparkFun   |
| 1    | **Printer Cable**                                    | 3 ft Cat5e (paired +5/GND) → Male RJ-45 ↔ 2× JST | Custom              |
| 1    | **Male ↔ Female Ethernet Cable (3 ft)**              | Replaces mid coupler; joins printer and skull    | Any                 |
| 1    | **Cat5e Patch Cable (stranded)**                     | Raw cable for custom leads                       | Any                 |
| 6    | **RJ-45 Pass-through Plugs**                         | Crimp connectors for custom cables               | Any                 |
| 1    | **RJ-45 Crimp Tool (pass-through)**                  | Assembly tool                                    | Any                 |
| 1    | **1000–2200 µF 10 V Low-ESR Capacitor**              | Mounted at printer Vcc / GND                     | Any                 |
| misc | **Zip-ties / Hot Glue**                              | Internal strain relief                           | —                   |

---

## ✅ Summary

This configuration keeps all modules detachable yet electrically robust:

* **Standard RJ-45 connectors** throughout.
* **2–3 twisted pairs in parallel** for 5 V / GND ensures low loss and minimal EMI.
* **Short pigtail panel port** avoids stress on skull electronics.
* **Male–female Ethernet cable** simplifies setup, eliminating the need for a separate coupler.
* Entire system can still be separated cleanly at the mid-cable disconnect for transport.
