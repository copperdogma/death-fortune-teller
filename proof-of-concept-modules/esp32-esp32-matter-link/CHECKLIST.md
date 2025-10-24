# Matter Master/Slave POC - Implementation Checklist

Use this checklist when building production projects based on this POC.

## 📋 Hardware Setup

- [ ] ESP32-S3 (Master) connected via USB for programming
- [ ] ESP32-C3 (Matter Node) connected via USB for programming
- [ ] UART wiring: S3 TX(17) → C3 RX(20), S3 RX(18) → C3 TX(21)
- [ ] GND connected between boards
- [ ] Both boards powered independently (no back-feeding)
- [ ] Verified both boards power on with LEDs

## 📋 Software Setup

### ESP-IDF Environment
- [ ] ESP-IDF v5.4.1 installed in `~/esp/esp-idf`
- [ ] ESP-Matter installed in `~/esp/esp-matter`
- [ ] Python environment working (`./install.sh` completed)
- [ ] Can run `idf.py --version` successfully

### Arduino IDE
- [ ] ESP32 board support installed
- [ ] ESP32-S3 board selected
- [ ] Correct COM port selected
- [ ] Serial Monitor baud set to 115200

## 📋 Code Implementation

### UART Protocol
- [ ] Frame format: `0xA5 LEN CMD PAYLOAD... CRC8`
- [ ] CRC8 using Dallas/Maxim polynomial (0x31)
- [ ] Command codes defined (0x01-0x7F)
- [ ] Response codes defined (0x80+)
- [ ] Timeout handling (1000ms)
- [ ] **ACK sent BEFORE slow operations** ⚠️

### Matter Node (C3)
- [ ] UART initialized (TX=21, RX=20, 115200 baud)
- [ ] LED initialized (GPIO 8)
- [ ] Signal GPIO initialized (GPIO 4) if needed
- [ ] Factory reset button (GPIO 9, 5s hold)
- [ ] Matter node created with custom name
- [ ] Endpoints created (trigger + 4 modes)
- [ ] Attribute callback registered
- [ ] Event callback registered (commissioning status)
- [ ] UART RX task created
- [ ] Mode sync task created
- [ ] Boot initialization to default mode
- [ ] **Mutual exclusivity logic implemented** ⚠️
- [ ] **Debouncing implemented (200ms)** ⚠️
- [ ] **Safety cleanup implemented (5s)** ⚠️

### Master (S3)
- [ ] UART initialized (TX=17, RX=18, 115200 baud)
- [ ] LED initialized (GPIO 2)
- [ ] CLI commands implemented
- [ ] Frame sending/receiving functions
- [ ] CRC8 calculation
- [ ] Incoming command handler
- [ ] Response timeout handling
- [ ] LED feedback patterns

## 📋 Testing

### UART Communication
- [ ] S3 CLI `hello` command works (C3 LED blinks)
- [ ] S3 CLI `ping` command works
- [ ] S3 CLI `trigger` command works
- [ ] S3 CLI `mode 0-3` commands work
- [ ] S3 receives commands from C3 when HomeKit triggers
- [ ] No timeout errors on any command
- [ ] CRC errors are detected and reported

### Matter Integration
- [ ] Device commissions successfully in HomeKit
- [ ] QR code displays in serial output
- [ ] Device appears in Home app
- [ ] Trigger endpoint works
- [ ] All 4 mode endpoints work
- [ ] Only one mode ON at a time (slow taps)
- [ ] Rapid taps eventually converge to correct state
- [ ] Force-close/reopen shows correct state
- [ ] Boot defaults to Little Kid mode
- [ ] Factory reset works (button or console)

### Edge Cases
- [ ] Rapid mode switching tested (may need force-close)
- [ ] Power cycle both boards - system recovers
- [ ] Disconnect/reconnect UART - no crashes
- [ ] HomeKit hub restart - device reconnects
- [ ] Multiple rapid triggers - no crashes
- [ ] Commission → factory reset → re-commission works

## 📋 Documentation

- [ ] README.md updated with project status
- [ ] POC-SUMMARY.md contains all patterns and learnings
- [ ] QUICK-REFERENCE.md has copy-paste code snippets
- [ ] Code comments explain critical patterns
- [ ] HomeKit limitations documented
- [ ] Setup instructions clear and complete
- [ ] Troubleshooting section comprehensive

## 📋 HomeKit Setup

- [ ] Device added to HomeKit via QR code
- [ ] Device renamed (e.g., "H-Death")
- [ ] Trigger outlet renamed
- [ ] Mode 0 outlet renamed (Little Kid)
- [ ] Mode 1 outlet renamed (Big Kid)
- [ ] Mode 2 outlet renamed (Take One)
- [ ] Mode 3 outlet renamed (Closed)
- [ ] Tested trigger from Home app
- [ ] Tested mode changes from Home app
- [ ] Verified UART commands sent to S3

## 📋 Known Limitations (Documented)

- [ ] HomeKit UI caching during rapid changes - **EXPECTED**
- [ ] Manual device/outlet renaming required - **EXPECTED**
- [ ] 1-10 second UI update delays - **EXPECTED**
- [ ] Force-close/reopen sometimes needed - **EXPECTED**

## 📋 Production Readiness

- [ ] All core functionality working
- [ ] All edge cases tested
- [ ] All patterns documented
- [ ] All limitations documented
- [ ] Setup process documented
- [ ] Troubleshooting guide complete
- [ ] Code comments comprehensive
- [ ] Ready for reuse in production projects

---

## ✅ POC Complete When:

All checkboxes above are checked, and you can:
1. Flash both boards
2. Wire them together
3. Commission to HomeKit
4. Control via Home app
5. See UART commands on S3 serial monitor
6. Understand all patterns and limitations

**This POC is your Matter development toolkit.** 🎃

---

*Use this checklist when building production projects to ensure you haven't missed any critical patterns.*

