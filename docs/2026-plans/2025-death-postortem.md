# 2025 Death Post-Mortem

See Apple Note "2025 Halloween Postmortem" -- it's more up to date.

Contents as of 20251101:


Hardware
- Wireless range! Also add wifi(?) antenna to battery motion sensor design
- Printer flakiness when printing logo. Would quickly print 200 lines of garbage instead of logo.  See ChatGPT tests/mitigations 
- Redo the capacitance sensor/shield with a proper RG174 cable, contiguous from foils to DuPont connector on the perfboard for maximum shielding 
- Simplify Death: short welcome, light up a “fortunes told here” sign, invitation to press button/touch whatever to get fortune. THEN start on fortune skit/print/goodbye. 
- Change printer cables to all RJ45 connectors and modular cables. Rj45 port on skull and another on perfboard. And use twisted 5V/GND pairs kept together so they cancel out each others magnetic fields (EM noise)
- Cancel button: for Jeff/anthony and death. Cancel the skit and reset to idle. Great for testing too. 
- Remote capture log? Whenever I see a bug I could capture it, note the time and record what went wrong. Or it could help with all of those things. Cuz stuff never goes wrong while testing, only in prod:)
- Make dedicated motion sensor for Jeff/Anthony. One with no cooldown that just fires out triggers nonstop. Intelligence relies on prop that ignores all triggers while playing put a cooldown. But ensure apple home supports this! Often the dumb thing has its own internal debounce/non flood logic so it’ll not send triggers or automations if there’s a lot of chatter. 
- Finger detector
    - Replace with a big button or something they have to touch for a very positive “I want this” experience. Probably best. 
    - Switch to more reliable(?) optical sensor or something where we’re not reliant on flaky capacitance readings (which aren’t great on the WROVER)
