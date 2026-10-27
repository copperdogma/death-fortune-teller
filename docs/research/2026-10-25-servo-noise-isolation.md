# Mechanical servo noise: controlled isolation

Cam identifies the remaining noise as mechanical buzzing/chatter from the jaw servo (not JBL interference). This is a motion/holding-control problem until measurements show otherwise.

## Existing evidence

- Verified card calibration is 1750..2100 µs, reversed, already matching the historical quiet range. Current mapping commands approximately 2094 µs closed, 2041 µs at idle-breath peak, 1950 µs at maximum speech opening.
- `ai-work/issues/servo-issues.md` records unresolved breathing buzz despite quiet fixed positions. Its redundant-update explanation was a hypothesis; native code already skips unchanged duty values.
- Native code uses 50 Hz hardware PWM with separate eye timer and power management disabled. No timer conflict or pulse-jitter fault is demonstrated.
- Integer-degree breathing steps are about 1.94 µs every 67 ms. Audible incremental corrections are plausible, not proven.

## Primary sources and choice

[Pololu: servo control interface in detail](https://www.pololu.com/blog/17/servo-control-interface-in-detail) explains pulse control and the usual loss of holding torque for analog servos when pulses stop. Applicability to the documented HS-125MG must be checked physically; do not assume every servo behaves identically.

Installed ESP-IDF `esp_driver_ledc/src/ledc.c` implements `ledc_stop` by disabling channel signal output and setting idle level; `ledc_update_duty` re-enables output. This supports a reversible diagnostic without changing timer calibration. Existing pending-update guard remains in place.

Compare normal breathing, a fixed calibrated closed target, and pulses stopped. The project documents an elastic jaw return. If hold is quiet but breathing chatters, investigate motion interpolation. If hold buzzes and off is quiet, investigate holding load/calibration and consider releasing pulses at rest. Pulse-off is diagnostic, not a demonstrated permanent fix.

Modes default to normal on reboot; explicit idle-only CLI selects hold/off. Both suppress animation until normal is restored. Eyes continue. Resume invalidates cached duty so the closed pulse is restored even if its value has not changed. Host tests cover pending-stop/resume, single stop, eye operation, overridden motion, and fresh breathing cadence. Hardware listening remains required.

## Controlled hardware result and bounded trajectory trial

Cam's hardware comparison found fixed hold quiet, normal breathing noisy during
motion, and quiet between breaths. This supports a movement-related cause rather
than continuous closed-position hunting; it does not distinguish normal gear or
motor noise from imperfect motion commands.

The [manufacturer HS-125MG specification](https://www.hiteccs.com/public/uploads/data_sheet/HCS_HS-125MG_Specsheetv2.2_1-1729888697.pdf)
lists a 5 us deadband, superseding the local copied 1 us figure for this
investigation. Sub-deadband command changes can still accumulate before a servo
corrects its position, so finer requested pulses do not guarantee silence.
[Pololu's speed and acceleration controls](https://www.pololu.com/docs/0J40/5.e)
provide an established basis for shaping position trajectories instead of
assuming more frequent writes alone improve movement.

The next bounded trial uses smoothstep easing with fractional angles only for
idle breathing and the gentle boot/manual check. It interpolates between the
existing integer-angle calibrated PWM duties, preserving each integer mapping,
travel endpoints, 2 s breathing halves, 100 ms peak hold, 7 s cadence, and
600 ms check halves. Speech mapping/smoothing and explicit overrides are
unchanged. Hardware PWM still quantizes to 16 bits at 50 Hz. The easing reduces
requested velocity at gesture boundaries; it does not modify servo deadband or
claim mechanically continuous motion.

Added dense 20 ms host samples checking monotonic opening/closing, bounded
travel, exact peaks/closure, intermediate pulses finer than old degree steps,
and unchanged hold timing. Existing hold/off/override tests remain applicable.
Root owns suite/build execution and physical listening. Noise improvement is
unverified until that comparison; a quiet motor or geartrain is not promised.
