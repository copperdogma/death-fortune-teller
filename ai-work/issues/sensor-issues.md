# Finger Sensor / Manual Calibration Issues Log

<!-- CURRENT_STATE_START -->
## Current State

**Domain Overview:**  
The capacitive finger sensor drives the NEAR/FAR trigger flow and manual calibration entry. It uses a single-input touch sensor (CAP_SENSE_PIN) sampled by `FingerSensor`, with stability thresholds configured in `config.txt`.

**Subsystems / Components:**  
- FingerSensor class — Degraded — Basic sampling works in firmware but hardware response is inconsistent  
- Manual calibration sequence — Unknown — Software state machine intact, hardware trigger unreliable  
- DeathController integration — In Progress — Controller now consumes finger snapshots; legacy handlers kept for feedback while migration continues

**Active Issue:** Physical finger sensor appears non-responsive; likely wiring/mechanical fault  
**Status:** Suspected hardware damage  
**Last Updated:** 2025-11-05  
**Next Step:** Inspect wiring/connector integrity, confirm sensor baseline readings with multimeter/serial stream before further software work.

**Notes:** Current commits re-enabled legacy finger event handling so manual calibration feedback remains, but sensor still fails to respond to touch. Expect hardware debugging or redesign if we keep this input at all.

**Open Issues (latest first):**
- 20251105 — Sensor reports no touch even with finger present; probable physical/wiring fault
- 20251105 — Manual calibration never triggers after controller refactor, even with strong touch

**Recently Resolved (last 5):**
- *(none yet)*
<!-- CURRENT_STATE_END -->

## 20251105 — Sensor Non-Responsive After Controller Integration

**Description:**  
During controller migration, the finger sensor stopped registering touches despite firmware still sampling the pin and legacy handlers firing. Re-enabling the old handler did not restore functionality, pointing to a likely physical disconnection or damaged sensor pad. Manual calibration and NEAR trigger flows depend on this signal.

**Environment:**  
- Hardware: CAP_SENSE_PIN wiring to touch sensor pad (single lead)  
- Firmware: `DeathController` consuming `FingerReadout`, legacy `handleFingerSensorEvents` retained  
- Config: Default `finger_detect_ms` / thresholds from `config.txt`

**Symptoms:**  
- No `Finger detected` logs when touching pad  
- Manual calibration sequence never starts  
- Sensor baseline appears flat in serial debug stream

**Investigation:**  
- Verified code paths still call `fingerSensor->update()` and both controller + legacy handlers  
- Confirmed controller tests pass (no hardware involved)  
- Reverted software changes to ensure no regression, sensor still unresponsive  
- Suspect physical damage or dislodged wiring, but hardware not yet inspected

**Open Questions:**  
1. Has the touch electrode or wire been bent/broken during refactor?  
2. Do raw sensor readings change at all (log `getRawValue()` via serial)?  
3. Should we consider replacing the sensor with a more robust proximity switch if controller migration continues?

**Next Steps:**  
- Inspect wiring/connectors when hardware is available  
- Log raw sensor values over serial to confirm ADC behaviour  
- Decide whether to keep or retire capacitive touch in final design
