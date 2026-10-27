# HS-125MG noise research and TwoSkulls comparison

Cam requested model-specific research after noting Death's more expensive servo is louder than Jeff/Anthony and already has O-ring mount isolation. No firmware, wiring or servo settings changed in this research pass.

## Models and limits of comparison

Death documents Hitec HS-125MG/32125S. Both TwoSkulls checkouts were searched; the current worktree's `docs/research/2026-10-16-perfboard-design-review.md:9` explicitly leaves actual servo model unidentified. A connected project-note search found no model either. Asked Cam for the printed model. Do not silently identify Jeff/Anthony as SG90 or MG90S based on price or housing alone.

No reliable model-specific acoustic benchmark or established widespread HS-125MG-is-noisy reputation was found in the bounded search. That is absence of evidence in this search, not proof of silence. This is an analogue servo; generic explanations about digital-servo high-frequency holding buzz do not automatically apply.

## Primary sources

- [Hitec HS-125MG product page](https://www.hiteccs.com/actuators/product-details/HS-125MG): analogue circuit, cored brushed motor, four metal gears plus a composite first gear, 4.8–6.0V. No acoustic rating.
- [Current Hitec data sheet](https://www.hiteccs.com/public/uploads/data_sheet/HCS_HS-125MG_Specsheetv2.2_1-1729888697.pdf): 5µs deadband, ±60° over900–2100µs, conventional contact potentiometer. Older project/retailer material says1µs, so revision/specification discrepancies remain; do not claim the actual unit's threshold was measured. Current manufacturer specification takes precedence as published data.
- [Official UK distributor analogue/digital explanation](https://hitec.uk/pages/explainer-circuits): analogue slow movements may lack smoothness. Its page contains a questionable20ms “deadband” label; use the model's own µs specification instead of repeating that number. No quantitative acoustic comparison provided.
- [Bruce Abbott's original servo current measurements](https://bhabbott.net.nz/Servo.html): slow servo-tester movements produce large starting-current pulses; HS-125MG included at roughly850mA peak in that setup. Author describes analogue pulsed motor drive and buzz under load. This is a firsthand electrical test, not an acoustic ranking against TwoSkulls.
- [TowerPro SG90](https://towerpro.com.tw/product/sg90-7/) and [MG90S](https://towerpro.com.tw/product/mg90s-3/): current official versions are listed as digital,1µs deadband; SG90 POM gears/9g, MG90S metal/13.4g. These are conditional comparisons only until Cam identifies his actual units. Older analogue SG90s and market variants cannot be inferred from these current pages.

## Strong project-specific leads

1. **Potential undervoltage after the series diode.** Death `docs/hardware.md:179–188` places the servo after a main-input Schottky and predicts4.6–4.8V from5V. The text's assurance that the servo operates normally there is not supported by Hitec's4.8V minimum. `docs/perfboard-assembly.md:54–66` likewise expects4.6–4.9V. If assembled as documented, actual connector voltage during movement deserves measurement. A5A adapter and bulk capacitors do not remove steady diode drop; actual voltage has not been measured. Do not change the shared supply or bypass protection based on this hypothesis.
2. **Different proposed power routing in TwoSkulls.** Draft-v4 `hardware/perfboard/draft-v4/README.md:63–65` and `wiring.csv:2–11,29–32` feed servo directly from external5V before D1, protecting only controller supply through D1. This is a planning draft, not verification of built skull wiring. It is a meaningful possible difference, not proof of cause.
3. **Very slow, small shaft movement and internal correction.** Measured breathing pulse excursion is53.1125µs over a2s opening, approximately5° of servo-shaft travel using the manufacturer's nominal pulse/travel relationship. Logical30° is an application coordinate, not30° measured shaft travel. Such a small trajectory relative to a published5µs deadband can plausibly cause stop/start motor corrections. Clean external PWM does not rule out this interaction or software update cadence; no direct internal error/current trace has been collected.
4. **Load, linkage, individual unit/geartrain.** O-rings attenuate mounting transmission but do not remove motor/gear sound or mechanical coupling through the jaw linkage. Current quiet closed hold does not establish quiet holding at every open position. The old log `ai-work/issues/servo-issues.md:664–667` records genericmicro andTowerProSG90 stable on the same test firmware/wiring while an HS-125MG intermittently ran away. That is a different historical symptom, not proof the current unit is defective or this model inherently noisy; archived speculative diagnoses are not accepted as established facts.

## Decision / next discriminating tests

Retain breathing. Prioritize actual voltage at the servo connector while moving, then the identical breathing profile with the jaw pull-link disconnected. A comparison on a verified regulated servo supply can separate voltage effects from mechanical demand; no such rewiring performed. If unloaded motion is also noisy on verified power, test a normal servo tester or a controlled trajectory/cadence comparison before condemning firmware or servo. Existing passive captures show clean20ms frames and commanded pulse steps at the ESP32 pad, not voltage quality at the servo connector. No faster refresh rate, servo disassembly, or unverified calibration expansion is justified by this research alone.
