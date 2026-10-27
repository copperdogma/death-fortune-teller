# Measure actual servo PWM before assigning the noise to hardware

Cam reports quiet fixed hold and noisy breathing motion. Fractional/eased motion did not audibly reduce the chatter. The expensive HS-125MG is louder than the cheaper TwoSkulls servos despite O-rings on its mounting. These observations do not rule out control-code timing, pulse glitches, load or servo mechanics. Keep breathing enabled.

## Primary-source research and decision

[Espressif GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/gpio.html) explicitly supports reading a GPIO that simultaneously outputs LEDC PWM, using `gpio_input_enable` without overwriting output configuration.

RMT RX could observe this pad, but classic ESP32 lacks receive ping-pong/DMA, and continuous PWM does not naturally reach an idle delimiter longer than its low interval. Buffer-full handling is not a clean bounded completion method. Public RMT APIs do not expose the allocated channel ID for selectively gating its matrix input. Reject a private-handle or output-interruption workaround.

[MCPWM capture documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/mcpwm.html) describes hardware-latched edge timestamps. Installed `esp_driver_mcpwm/src/mcpwm_cap.c` confirms capture creation enables the GPIO input/matrix path without replacing the LEDC output route; deletion disconnects only the capture input. Preserve original pull settings. Classic ESP32 capture uses APB frequency directly; query actual resolution (normally80MHz,12.5ns/tick), rather than assuming requested divider support.

## Bounded passive experiment

An explicit idle-only probe allocates one capture timer and two channels on GPIO23: rising-only and falling-only, prescale1. Separate latches provide20ms between successive events of each polarity instead of requiring one latch to survive the2ms high-pulse interval. The callback records hardware ticks, polarity and CPU receipt time into a64-event internal-memory ring protected across possible ISR cores. It performs no logging, allocation, GPIO writes or LEDC writes. The control task drains into PSRAM history and records changed successfully programmed command duties. Capture ends after8seconds or when the show ceases to be idle.

After disabling/freeing capture interrupt resources, sort by modular hardware timestamps, compute complete pulse widths/periods, and print at most16 representative cycles and16command records plus summary and at most16out-of-range pulse records. Abort skips analysis/traces so a newly starting show is not delayed. Restore a previously disabled GPIO input buffer with the same `gpio_ll_input_disable` helper used by the SDK's private wrapper: this changes only its input-enable bit, unlike generic GPIO reconfiguration which could overwrite LEDC routing. Log output routing/mux/enable/drive/pulls/input comparison. No output pulse is intentionally removed or changed by this probe.

Compare quiet hold against normal breathing. Fixed-hold width/period variation or out-of-range complete pulses would support an electrical/control timing investigation. Compare moving measured widths against command steps and update times. A command's software write time is not the exact latch time; hardware may apply it on the following PWM cycle. CPU receipt time is supplementary alignment information, not the primary edge timestamp.

## Limitations and checks

Capture registers are single latches, not FIFOs. Long ISR delays may overwrite events; ring overflow, repeated edges and long captured gaps must be reported as capture quality problems, not proof of missing physical PWM. Sorting handles differing cross-core callback dispatch order within the8-second/half-wrap bound. This measures digital pad timing, not signal amplitude at the servo connector, supply droop, motor current or mechanical force.

Pure host tests cover exact widths/periods, deliberate width variation, ISR order reversal, timer wrap and capture gaps. Native build and hardware comparison remain root-owned; no measured result is yet claimed.
# Device results — 2026-10-25 23:32

Installed observer image SHA256 `5b2d61fb7a498f5c80552c5415aff7e997e6ecd725c33fc777e2f2407db83f11`; host sanitizer suites, pure capture-analysis tests and Death flow/assets/fortune suites passed. App-only flash verified, replacement SD initialized and startup PCM matched; speaker connected on its second remembered attempt.

Eight-second hold: 802 edges, 400 complete measured periods, all1600000ticks/20ms at80MHz. High width uniformly167530ticks/2094.125µs, zero adjacent change, drops, gaps or invalid widths. GPIO output configuration and original input-enable state restored/unchanged after cleanup.

Eight-second normal breathing: 801 edges, 399 complete periods, all1600000ticks/20ms. High widths163281..167530ticks (2041.0125..2094.125µs). Largest adjacent change245ticks/3.0625µs; largest recorded command change10duty counts corresponds3.0517578125µs. Difference is less than one80MHz capture tick. No ring drop, history truncation, capture gap or invalid width; one leading/unpaired edge is consistent with acquisition starting mid-pulse. Output configuration unchanged after cleanup. Full structured results in `docs/validation/2026-10-25-servo-pwm-capture.json`.

These samples show no malformed pulse widths or unexpected frame/pulse jitter. They do **not** rule out the commanded trajectory/update cadence as an audible contributor, independently calibrate APB time, measure voltage/current at the servo connector, or establish a mechanical cause. Breathing remains enabled. Do not conclude that an expensive servo is inherently noisy from this measurement alone.

Bounded driver audit additionally found fixed-duty writes use one step/one cycle/zero fade scale, not a motion fade. Keep the classicESP32 pending guard: LL warns against reprogramming before duty_start clears. `max_pending_us` spans polling observations and can include scheduler/UART delay; it is not measured continuous hardware busy time. Historical ServoESP32 first-channel default used high-speed LEDC, whereas native TwoSkulls/Death use low-speed; no backend change was made because current captures did not show waveform faults. Periodic position scheduling and hardware output should be treated separately in future experiments.
