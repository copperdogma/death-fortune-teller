# Skull Jaw Mechanism — Through-Axle D-Shaft Drive

20251101: Created via GPT5 convo: https://chatgpt.com/g/g-p-68992f0599d081918441e6fcb4fac8a9-electronics/c/69065447-d9d8-8329-9ec1-ce67feb02317


NOTE: Basically "do not do this";) Aria and I agreed the complexity of not knowing if another wireless thing is working is NOT worth the potential benefits.


## AI Writeup of Convo

# Optional Wireless Printer Concept (Not Implemented)

## Overview
This document outlines a potential design for making the **Death Fortune Teller** project’s thermal printer wireless.  
The concept was explored for completeness but **rejected due to reliability concerns**.  
The wired implementation remains the official and supported configuration.

---

## Concept Summary
The goal was to decouple the thermal printer from the main ESP32-WROVER controller using a dedicated **ESP32-C3 SuperMini** acting as a wireless print server.  
The WROVER would transmit print data over a wireless protocol, and the C3 would forward it to the printer via the printer’s **3.3 V TTL interface**.

---

## Proposed Hardware

| Component | Description | Notes |
|------------|--------------|-------|
| **ESP32-C3 SuperMini** | Wireless “print server” microcontroller | Wi-Fi + ESP-NOW capable |
| **5 V Power Node** | Dedicated 5 V supply (≥ 2 A) | Isolated from logic power except common ground |
| **Printer Connector** | 4-pin (5 V, GND, TX, RX) | Direct TTL connection, no level shifting needed |
| **Bypass Header** | 3-pin or DPDT toggle | Instantly swap printer UART between WROVER (wired) and C3 (wireless) |
| **Bulk Capacitors** | 1000 – 2200 µF electrolytic + 0.1 µF ceramic | Mounted near printer input to absorb surge current |

---

## Firmware Roles

### ESP32-WROVER (Main Controller)
- Generates ESC/POS commands and print job data.
- Sends print payloads wirelessly to the printer’s C3 server.
- Handles retries and status acknowledgments.

### ESP32-C3 SuperMini (Print Server)
- Receives print packets over wireless (ESP-NOW or Wi-Fi TCP).
- Buffers incoming chunks, acknowledges receipt, and streams to the printer’s UART.
- Optionally hosts a simple HTTP API (`/print`, `/status`) for LAN testing.

---

## Communication Options

### 1. **ESP-NOW**
Peer-to-peer wireless link between the WROVER and C3.  
- **Pros:** No router or network dependency, low latency, highly reliable.  
- **Cons:** Not discoverable on LAN; can’t print from phone or PC.

### 2. **Wi-Fi TCP / HTTP**
The C3 acts as a simple TCP server (port 9100-style) or lightweight HTTP endpoint.  
- **Pros:** Usable from WROVER or any LAN device; flexible.  
- **Cons:** Depends on Wi-Fi stability; adds latency and possible reconnection issues.

### 3. **MQTT (Optional)**
Publish/subscribe model for print jobs via a local broker.  
- **Pros:** Reliable message delivery and monitoring.  
- **Cons:** Requires broker service; extra complexity for minimal gain.

---

## Integration with Matter and HomeKit
Neither **Matter** nor **HomeKit** currently support printers as native device types.

- **HomeKit:** No “Printer” service in HAP; AirPrint/IPP is handled separately by iOS/macOS.
- **Matter:** Designed for device *control* and *state synchronization*, not data transfer.  
  It cannot transmit raw print data and has no “Printer cluster.”
- **Workaround:** A virtual “Print” switch could trigger a job via the WROVER, but actual print data would still travel over ESP-NOW or TCP.

---

## Reasons for Rejection
Although technically feasible, this approach was **not adopted** for the following reasons:

1. **Reliability** – The wired TTL link is already proven and stable; wireless adds new failure modes.
2. **Power Isolation** – The printer’s high surge current can cause brownouts on shared rails when wireless MCU power is combined.
3. **Wi-Fi Stack Conflicts** – The existing Matter integration already pushes the WROVER’s Wi-Fi stack; additional sockets could degrade performance.
4. **Complexity vs. Benefit** – Wireless printing offers little practical gain for a fixed installation.

---

## Future Considerations
If native wireless printing becomes desirable:
- Use a **Raspberry Pi Zero 2 W** running **CUPS/AirPrint** connected via USB-TTL.
- The Pi would handle **standard IPP printing** over Wi-Fi while maintaining wired TTL to the thermal printer.
- The WROVER could send jobs to the Pi via HTTP or TCP, enabling true cross-ecosystem printing (including from iOS).

---

## Conclusion
A wireless print server is technically achievable using an ESP32-C3, but the added complexity and potential instability outweigh any convenience gained.  
For robustness and show reliability, the **wired TTL connection remains the preferred configuration** for the Death Fortune Teller project.
```

