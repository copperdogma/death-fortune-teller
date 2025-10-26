# Servo Smoothness Issues Log

<!-- CURRENT_STATE_START -->
## Current State

**Domain Overview:**  
The servo controller subsystem manages jaw movement for the Death Fortune Teller skull using a HS-125MG servo. It handles audio-synchronized jaw animation and a breathing idle cycle. Recent focus has been on achieving perfectly smooth motion comparable to the TwoSkulls reference implementation, which uses Arduino Servo.h and produces silky-smooth movement, while our ESP32Servo-based implementation exhibits noticeable stepping/jerky motion.

**Subsystems / Components:**  
- ServoController class — Partial — Functional but motion is stepped/jerky compared to reference
- smoothMove() function — Broken — Produces visible stepping despite timing interpolation
- setPosition() method — Working — Correctly controls servo position within constraints

**Active Issue:** Servo stepping/jerky motion in breathing cycle  
**Status:** Active  
**Last Updated:** 2025-01-23  
**Next Step:** Research Arduino Servo.h implementation to understand smooth motion mechanism

**Open Issues (by latest first):**
- 20250123-NOW — Servo stepping/jerky motion during breathing cycle

**Recently Resolved (last 5):**
- None
<!-- CURRENT_STATE_END -->

## 20250123-UNKNOWN: Servo Stepping/Jerky Motion During Breathing Cycle

**Description:**  
The servo breathing animation displays noticeable stepping/jerky motion when using ESP32Servo library, while the TwoSkulls reference implementation (using Arduino Servo.h) produces perfectly smooth motion with identical code logic. Despite implementing timing-based interpolation with easing functions and trying various update rates (10ms, 20ms, continuous), the motion remains visibly stepped.

**Environment:**  
- Platform: ESP32-WROVER on perfboard
- Framework: PlatformIO with Arduino framework
- Servo Library: ESP32Servo@3.0.9
- Reference: TwoSkulls uses Arduino IDE with <Servo.h>
- Servo: Hitec HS-125MG

**Evidence:**
- TwoSkulls implementation is "perfectly smooth" per user testing
- Current implementation is "jerky" and shows "visible steps"
- Both use identical smoothMove() timing logic (startTime/currentTime/endTime)
- Both use 20ms delays and linear interpolation
- Key difference: TwoSkulls uses `servo.write(servoPin, degrees)` while ESP32Servo uses `servo.write(degrees)`

### Step 1 (20250123-UNKNOWN): Initial Analysis and Code Comparison
**Action**: Compared TwoSkulls servo_controller.cpp with current src/servo_controller.cpp implementations.

**Result**: 
- Identical smoothMove() logic: timing-based interpolation with 20ms delays
- Key difference in setPosition():
  - TwoSkulls: `servo.write(servoPin, constrainedDegrees)`
  - Current: `servo.write(constrainedDegrees)` (ESP32Servo only supports single-arg)
- Both use servo.attach(servoPin) with no special pulse width configuration

**Notes**:
- ESP32Servo appears to be a different library than Arduino's Servo.h
- The write() signature difference suggests different PWM generation mechanisms
- Attempted fixes (writeMicroseconds, easing, update rates) suggest the issue is in how ESP32Servo generates PWM rather than our interpolation logic

**Next Steps**: Research Arduino Servo.h implementation to understand PWM generation differences and find migration path to smoother control.


**20251025 Research by GPT5**

I did some digging into both the classic Servo library (Arduino AVR/Arduino core) and the ESP32Servo library for ESP32, with the goal of finding exactly how the Arduino library achieves smooth motion (or appears to) and where the ESP32 library may diverge (leading to the stepped/jerky behaviour you observe). Here is a summary of what I found, followed by my analysis of why you may be seeing the difference.

⸻

1. Arduino Servo library (AVR / Arduino core) — key implementation points

I couldn’t locate a full easily-searchable version of the Servo.cpp inside the ESP32 Arduino core (because the ESP32 core often uses its own hardware PWM implementations rather than the original AVR approach). But I found enough commentary and excerpts of the original AVR-library to infer how it works. (A published analysis: “Arduino’s Servo Library: Angles, Microseconds, and ‘Optional’ Command Parameters” covers this.  ￼)

Here are the key aspects:
	•	The library exposes Servo::write(angle) (0-180 degrees) and Servo::writeMicroseconds(us) (direct pulse width) APIs.  ￼
	•	Internally, write(angle) maps the angle to a pulse width between a minimum and maximum microsecond value. The default min/max are 544 µs and 2400 µs if you use attach(pin) without specifying min/max.  ￼
	•	Example mapping (approx):

pulseWidth_us = map(angle, 0, 180, min_us, max_us);  

Where min_us = 544, max_us = 2400 (unless overridden via attach(pin, min_us, max_us)).  ￼

	•	For AVR platforms, the library sets up a timer interrupt (or multiple timers) that generate servo pulses at a refresh interval (typically ~20 ms, i.e. ~50 Hz) per servo channel. In the servo library source we see macros like:

#define usToTicks(_us) ((clockCyclesPerMicrosecond()* _us) / 8)  // etc.  

￼

	•	Because the library uses timer interrupts and distributes multiple servo pulses in sequence, it effectively multiplexes the pulses so that each servo gets its pulse in each 20 ms window, and the rest of the time is idle.
	•	The write/update to the pulse width simply writes a value into a data structure; the actual hardware pulses continue to be generated in the background via the timer ISR. There is no interpolation (ramping) built into the base library: the library simply sets the target pulse width and the servo receives that width on the next refresh.
	•	Because the servo internal mechanism (gear + potentiometer feedback) takes some time to physically move the shaft, the motion can appear smooth if the changes in angle are small and the refresh is frequent relative to the physical movement. Users essentially rely on the mechanical inertia of the servo to smooth the motion.
	•	The article notes:
“The write method simply maps the degrees to microseconds and calls the writeMicroseconds method anyway.”  ￼
	•	One relevant snippet (from the AVR library) is:

#if defined(ARDUINO_ARCH_AVR)
…
#define usToTicks(_us) (( clockCyclesPerMicrosecond()* _us) / 8)
…
static servo_t servos[MAX_SERVOS];
…

This shows the lower‐level interrupt/tick conversion.  ￼

	•	Because the library uses hardware timers and interrupt service routines (ISRs) which run even if the main loop is busy, the servo pulses are fairly regular and jitter is kept low (assuming nothing else blocks interrupts for long). That regularity helps motion appear smooth.

Summary for Arduino Servo library
	•	Maps degrees → microsecond pulse width via map().
	•	Uses hardware timers and interrupts to send a steady ~50 Hz pulse train.
	•	No built‐in interpolation/ramping; smoothness depends on how often write() is called and mechanical inertia.
	•	Good precision (on AVR) because pulses delivered at regular refresh intervals; minimal steps between successive widths if angle changes small.

⸻

2. ESP32Servo library (ESP32) — key implementation points

Here’s what I found in the library source (via GitHub and documentation) for ESP32Servo, which tries to mimic the Arduino API.  ￼

Key points:
	•	From the source excerpt (ESP32Servo.cpp) we see:

Servo::Servo()
{
  REFRESH_CPS = 50;
  this->ticks = DEFAULT_PULSE_WIDTH_TICKS;
  this->timer_width = DEFAULT_TIMER_WIDTH;
  this->pinNumber = -1;
  this->min = DEFAULT_uS_LOW;
  this->max = DEFAULT_uS_HIGH;
  this->timer_width_ticks = pow(2,this->timer_width);
}

￼

	•	The comments in the code say:
“Different servos require different pulse widths … an approximately 500-2500 microsecond pulse every 20ms (50Hz). … The ESP32 PWM timers allow us to set the timer width (max 20 bits). Thus … count = (pulse_high_width / (pulse_period/2**timer_width))”  ￼
	•	The attach logic:

int Servo::attach(int pin)
{
  return (this->attach(pin, DEFAULT_uS_LOW, DEFAULT_uS_HIGH));
}
int Servo::attach(int pin, int min, int max)
{
  // …
  this->min = min;
  this->max = max;
  // setup LEDC / PWM for ESP32
  pwm.setup(pin, REFRESH_CPS, timer_width);  
  pwm.write(pin, ticks);  
  this->pinNumber = pin;
  return pin;
}

(simplified)  ￼

	•	The write method (excerpt) roughly:

void Servo::write(int value)
{
  int pulseWidth_us;
  if (value < MIN_PULSE_WIDTH) {
    // treat value as microseconds
    pulseWidth_us = value;
  } else {
    // treat value as degrees
    pulseWidth_us = map(value, 0, 180, min, max);
  }
  this->writeMicroseconds(pulseWidth_us);
}

And in writeMicroseconds:

void Servo::writeMicroseconds(int value_us)
{
  if (value_us < min) value_us = min;
  else if (value_us > max) value_us = max;
  ticks = (long)( (value_us) / (pulse_period_us / timer_width_ticks) );
  pwm.write(pinNumber, ticks);
}

(where pulse_period_us is e.g. 20000 µs for 50 Hz)  ￼

	•	The important difference: it uses the LEDC (ESP32 hardware PWM) API (ledcSetup, ledcWrite, etc) which is not interrupt‐driven in the same way as the AVR library, but uses hardware PWM channels.

Summary for ESP32Servo library
	•	It maps degrees → microseconds → tick count using a formula based on hardware timer width.
	•	It uses the ESP32 LEDC hardware PWM once to configure frequency and duty cycle (no per‐servo ISR multiplexing).
	•	Each write() call effectively updates the duty (tick count) for the PWM channel corresponding to the servo pin.
	•	No built‐in interpolation/ramping either; you set a target angle and the library translates that to the appropriate duty.

⸻

3. Comparison & implications for smoothness

Based on the above, here are the key differences and how they might translate to the behaviour you observe (Arduino library “perfectly smooth” vs ESP32Servo “stepped/jerky” under identical input sequences).

a) Refresh mechanism / pulse generation
	•	Arduino library uses a fixed refresh interval (~20 ms) via timer interrupts. Every servo gets one pulse per refresh. Because this is hardware‐timer based and interrupts ensure regular delivery, successive pulses for a given servo are evenly spaced.
	•	ESP32Servo uses hardware PWM LEDC channels which continuously output the configured duty at the set frequency (50 Hz) without needing interrupts to start/stop each pulse. This is good in principle — less CPU overhead — but it does mean that the resolution of the hardware PWM channel (timer width + tick resolution) and how frequently you call write() become more significant.

b) Mapping resolution
	•	On Arduino, the mapping is simple: degrees → µs, and the timer tick conversion is tuned for the AVR timers. Because the servo is physically slow, small changes in pulse width may not result in noticeable jumps, leading to seemingly smooth motion.
	•	On ESP32Servo, the mapping is also degrees → µs → ticks (for LEDC). But the tick resolution is determined by timer_width (e.g., a 16-bit width means some quantisation of pulse width). Also, if your input sequence jumps angle values significantly, the change in duty will also be significant and instantaneous (from one refresh to the next). The “stepped” appearance may come from the servo physically moving, but the library causing discrete changes in pulse width without smoothing.

c) Interpolation / ramping
	•	Neither library (by default) implements interpolation (i.e., ramping from current angle to target angle over many steps). Motion smoothness is thus chiefly dependent on:
	1.	how frequent you call write() (if you update many times per refresh cycle)
	2.	how large the jump in angle is per write()
	3.	the servo’s own mechanical inertia/filtering
	•	Because the Arduino library likely had simpler loads and lower performance demands historically, many users implicitly called write() slowly (delay between writes) and hence got smooth motion. With ESP32, you may be calling write() with large increments or quickly, making the motion visibly “stepped”.

d) Jitter and update frequency
	•	On AVR the pulses are very regular as they are handled by an ISR; even if the main loop is busy, the timer still fires. That regularity reduces jitter and gives consistent motion.
	•	On ESP32, though LEDC is hardware PWM and thus regular, the actual servo movement depends on how smoothly you transition the target. If you call write() with big jumps and fast intervals, the hardware will simply adjust to the new duty for the next cycle — but the servo may catch up only slowly, causing a “stair‐case” effect.

e) Default pulse width ranges & calibrations
	•	The Arduino library defaults of 544 µs to 2400 µs give a larger “span” of possible pulse widths; if you use the default and many servos only require 1000-2000 µs, the mapping becomes less granular (i.e., more µs per degree), which can smooth out visible steps (because each degree change results in a relatively small µs change).
	•	On ESP32Servo you might be specifying attach(pin, 1000, 2000) or using defaults; but if the tick resolution is coarse, you might get fewer distinct tick values for the range, making motion appear more stepped.

⸻

4. Why you might see “perfectly smooth” on Arduino and “jerky/stepped” on ESP32Servo

Given all of the above, here’s a likely explanation in your context:
	•	On Arduino (or when using the classic Servo library on compatible boards) the combination of regular refresh, relatively fine control, and mechanical smoothing may mask discrete jumps — if you only call write() when angle changes slowly (or you include a delay between updates). The servo motion naturally filters the discrete pulses.
	•	On ESP32 with ESP32Servo, if your input sequence is calling write() with many small increments (or maybe even large increments) very quickly, then each update leads to a discrete jump in duty width which the servo tries to follow. Because there is no interpolation inside the library, and perhaps because the tick resolution or update timing is coarser than the servo’s mechanical time constant, you see visible stepping.
	•	In effect: you may be changing target angles faster than the servo and the driver can “smooth out”.
	•	Also if your update frequency (calls per second) is high and your duty resolution is coarse, each increment may be just a few ticks, so the servo moves a little, waits until the next pulse, moves a little again — which looks like stepping.
	•	Finally, the difference may be perceptual rather than strictly library‐driven: small jumps at high speed vs slower updates will feel different. The Arduino library might have been worked with slower updates historically; your ESP32 setup might be pushing faster changes.

⸻

5. Specific code snippets — how mapping is done

Here are key snippets (from ESP32Servo; for Arduino we only have commentary but the mapping logic is essentially the same).

ESP32Servo write method (simplified)

void Servo::write(int value)
{
  int pulseWidth_us;
  if (value < MIN_PULSE_WIDTH) {
    // treat as microseconds
    pulseWidth_us = value;
  } else {
    // treat as degrees: 0-180 maps to min-max
    pulseWidth_us = map(value, 0, 180, min, max);
  }
  writeMicroseconds(pulseWidth_us);
}

void Servo::writeMicroseconds(int value_us)
{
  if (value_us < min) value_us = min;
  else if (value_us > max) value_us = max;

  // Convert to tick count for LEDC PWM
  ticks = (long)( (value_us) / (pulse_period_us / timer_width_ticks) );
  pwm.write(pinNumber, ticks);
}

(from GitHub source)  ￼

Arduino library mapping commentary

“The write method simply maps the degrees to microseconds and calls the writeMicroseconds method anyway.”  ￼
And:
“If you use write(angle) and you didn’t override the min/max, the default min and max are 544 and 2400 µs.”  ￼

So essentially: degrees → µs → hardware pulses.

⸻

6. What isn’t happening (in both libraries)
	•	Neither library by default does ramping interpolation (e.g., smoothly stepping from current angle to target over many small increments inside the driver). If you want ease-in/ease-out, you must implement that yourself (or use a library built for smoothing, e.g. ServoEasing).  ￼
	•	Neither library is aware of the physical speed or load of the servo; they just set the target pulse width immediately.

⸻

7. Recommendations (to make the ESP32 behaviour smoother)

Since you are seeing jerky/stepped motion with the same input sequence, here are steps to improve smoothness on the ESP32 using ESP32Servo (or even switching libraries):
	•	Ensure you are updating the servo angle slower, i.e., don’t call write() with large jumps in angle; instead increment the angle in small steps (e.g., 0.5° or 1° every X ms) so the servo has time to move.
	•	Insert a small delay between successive write() calls so you don’t overload the servo with too many updates too fast.
	•	If possible, increase the tick resolution (in ESP32Servo you might adjust timer_width) so that the PWM duty change granularity is finer; this can reduce visible steps.
	•	Explicitly specify min and max pulse widths in attach(pin, min_us, max_us) that match your servo’s actual spec (e.g., 1000-2000 µs) rather than defaults (544-2400 µs); this gives better mapping and may reduce unintended coarse jumps.
	•	If more advanced, implement interpolation in software: when you get a new target angle, compute small increments from current angle to target over time (e.g., every 10 ms) and call write() for each increment.
	•	Check hardware factors: power supply stability, servo mechanical play, PWM frequency (50 Hz is standard). Sometimes jerky motion is caused by power sag or mechanical stiction rather than library.
	•	Compare latency: maybe the ESP32 servo update has some lag or coarser hardware tick size causing visible steps.

⸻

8. Final diagnosis

In conclusion, the reason your Arduino version appears “perfectly smooth” while the ESP32Servo version shows “stepped/jerky” is most likely because:
	•	The Arduino library’s refresh/ISR mechanism and perhaps slower update rate mask the discrete jumps, making motion appear fluid.
	•	On the ESP32, the PWM hardware driver in ESP32Servo is doing exactly the same physical task (set duty for 50 Hz), but because you are perhaps updating more aggressively and the duty resolution is coarser (or your angle jumps are larger) the motion becomes visibly stepped.
	•	The lack of built-in interpolation means that when target positions change abruptly, the servo must physically traverse the jump; if you gave it more time and smaller increments, it would look smoother.

If you like, I can pull the full source code of the Arduino Servo.cpp (from AVR core) and of ESP32Servo side-by-side, highlight literal code differences (tick resolution, mapping constants, timer setup) so you can see the concrete difference at instruction level. Would you like me to fetch that?


### Step 2 (20250123-UNKNOWN): Analysis of GPT-5 Research on Servo Library Differences
**Action**: Reviewed comprehensive GPT-5 analysis comparing Arduino Servo.h vs ESP32Servo implementations.

**Result**: 
Key findings:
- **Both libraries use identical mapping logic** (degrees → microseconds → hardware pulses)
- **Neither library has built-in interpolation** - both rely on mechanical inertia for smoothness
- **Critical difference identified**: ESP32Servo uses LEDC hardware PWM with tick quantization
- **Root cause hypothesis**: ESP32Servo's tick resolution may be coarser, causing discrete jumps in duty cycle
- Arduino library defaults (544-2400µs) provide larger span = more µs per degree = smoother perceived motion
- ESP32Servo's `timer_width` parameter determines tick resolution granularity

**Notes**:
- The GPT-5 analysis suggests the issue is **not** in our interpolation logic (which is correct)
- The problem is likely in ESP32Servo's tick resolution quantization when converting microseconds to PWM ticks
- ESP32Servo's formula: `ticks = (value_us) / (pulse_period_us / timer_width_ticks)` introduces quantization
- With 16-bit timer_width (default), we get 65536 possible tick values for 0-20000µs period = ~0.3µs resolution
- However, if angle changes result in small tick increments, the servo sees discrete duty changes
- The stepping we observe is likely the servo responding to these quantized duty changes faster than mechanical inertia can smooth them

**Next Steps**: Experiment with ESP32Servo's timer_width parameter to increase tick resolution, or implement finer microsecond-level control by using writeMicroseconds() with sub-degree precision in smoothMove().

### Step 3 (20250123-UNKNOWN): Implemented Microsecond-Level Precision
**Action**: Modified smoothMove() to interpolate at microsecond level instead of degree level.

**Result**: 
- Mapped degrees to microseconds at start/target (1000-2000µs for 0-180 degrees)
- Interpolated pulse widths in microsecond increments
- Used servo.writeMicroseconds() directly instead of setPosition()
- Updated currentPosition tracking to match

**Notes**:
- Rationale: GPT-5 research identified quantization as root cause
- By interpolating at 1000µs-2000µs range instead of 0-180 degrees, we get 1000 discrete values vs 180
- This should reduce stepping by providing finer intermediate positions
- Timing unchanged: 20ms delays, linear interpolation
- Awaiting user testing to validate improvement

**Next Steps**: Test firmware and report if microsecond-level precision improves smoothness. If still stepped, investigate ESP32Servo's timer_width parameter for increasing PWM resolution.

### Step 4 (20250123-UNKNOWN): Microsecond Approach Failed - Analyzing Alternatives
**Action**: Tested microsecond-level precision approach.

**Result**: 
- Motion is WORSE - more stepped than before
- Servo opening wider than expected (positioning error)
- Approach rejected as ineffective

**Notes**:
- Microsecond interpolation did not solve the quantization issue
- The wider opening suggests mapping (1000-2000µs) may be incorrect for this servo
- Need to revert to working state and try different approaches

**Potential Approaches Based on GPT-5 Research:**

1. **Increase ESP32Servo tick resolution** - Modify timer_width parameter
   - ESP32 supports up to 20-bit timer width
   - Current default likely 16-bit (65536 ticks)
   - Increasing to 20-bit = 1,048,576 ticks for 20000µs period = 0.019µs resolution
   - **Pros**: Addresses root cause (quantization), library-level solution
   - **Cons**: Requires modifying ESP32Servo internals or finding configuration method

2. **Match Arduino defaults (544-2400µs)** - Use wider pulse width range
   - GPT-5 noted Arduino's default 544-2400µs gives more µs per degree
   - Current implementation likely uses 1000-2000µs (standard servo range)
   - **Pros**: Proven by Arduino's smooth behavior, simple change
   - **Cons**: May exceed servo's mechanical limits

3. **Reduce update frequency** - Call write() less frequently
   - Research suggests Arduino benefits from slower updates
   - Try 40ms or 50ms delays instead of 20ms
   - Give servo more time to physically respond between commands
   - **Pros**: Works with mechanical inertia, simple timing change
   - **Cons**: Slower motion, may not solve stepping if quantization is issue

4. **Implement proper easing with larger steps** - Reduce number of intermediate positions
   - Instead of updating every 20ms with 1° increments
   - Update every 50-100ms with 2-5° increments
   - Let mechanical inertia smooth between updates
   - **Pros**: Leverages servo's physical smoothing, reduces PWM updates
   - **Cons**: May appear choppier, defeats purpose of smooth motion

5. **Switch to Arduino Servo.h via Arduino IDE** - Use proven working library
   - TwoSkulls uses Arduino IDE with Servo.h
   - PlatformIO could potentially use same library
   - **Pros**: Guaranteed to work (proven by TwoSkulls)
   - **Cons**: Breaks PlatformIO workflow, requires Arduino IDE or complex library port

**Most Promising Approach**: 
**#1 - Increase ESP32Servo tick resolution** by configuring timer_width to 20-bit (maximum). This directly addresses GPT-5's root cause analysis (quantization) and should provide sufficient resolution to eliminate visible stepping while keeping our current PlatformIO setup.

**Next Steps**: Research how to configure ESP32Servo's timer_width parameter, or find alternative servo library with finer PWM resolution.

### Step 5 (20250123-UNKNOWN): Increased Timer Width to 20-Bit Resolution
**Action**: Added servo.setTimerWidth(20) call in initialize() function to maximize PWM resolution.

**Result**: 
- ESP32Servo supports setTimerWidth() method with range 10-20 bits
- Changed from default 10-bit (1024 ticks) to maximum 20-bit (1,048,576 ticks)
- Resolution improvement: 1024x finer PWM quantization
- With 20-bit timer: 1,048,576 ticks for 20000µs period = 0.019µs resolution
- Reverted smoothMove() to original linear interpolation (removed microsecond approach)

**Notes**:
- According to ESP32Servo source, must call setTimerWidth() BEFORE attach()
- 20-bit resolution should provide sufficient granularity to eliminate visible stepping
- This directly addresses GPT-5's identified root cause (PWM quantization)
- Simple one-line change: servo.setTimerWidth(20) before servo.attach()

**Next Steps**: Test firmware and report if 20-bit timer width eliminates stepping. If still present, investigate approach #2 (matching Arduino defaults) or #3 (slower update frequency).

### Step 6 (20250123-UNKNOWN): 20-Bit Timer Width Failed - Still Stepping
**Action**: Tested 20-bit timer width with maximum PWM resolution.

**Result**: 
- Stepping still present despite 1024x resolution increase
- Quantization is NOT the root cause
- Hypothesis from GPT-5 research: "if your update frequency (calls per second) is high, each increment may be just a few ticks, so the servo moves a little, waits until the next pulse, moves a little again — which looks like stepping"

**Key Insight**:
The GPT-5 research highlights: "ESP32Servo uses hardware PWM LEDC channels which continuously output the configured duty at the set frequency (50 Hz) without needing interrupts to start/stop each pulse. This is good in principle — less CPU overhead — but it does mean that... how frequently you call write() become more significant."

**Analysis**:
- Current: Updating every 20ms with small angle increments
- Servo physical response time may be slower than our 20ms update rate
- Result: Servo sees discrete position commands faster than it can respond mechanically
- This creates visible stepping as servo tries to "catch up" to rapid discrete commands

**Next Steps**: Implement slower update frequency (40-50ms delays) to give servo time to physically respond between commands, leveraging mechanical inertia as Arduino library does.

### Step 7 (20250123-UNKNOWN): Slower Update Rate Made Stepping Worse
**Action**: Changed delay from 20ms to 50ms to slow update rate.

**Result**: 
- Stepping is WORSE with slower updates
- This confirms we're on the wrong track
- The problem is not update frequency

**Critical Insight from GPT-5 Research**:
"The important difference: it uses the LEDC (ESP32 hardware PWM) API... which is not interrupt-driven in the same way as the AVR library, but uses hardware PWM channels... Each write() call effectively updates the duty (tick count) for the PWM channel... No built-in interpolation/ramping either; you set a target angle and the library translates that to the appropriate duty."

**Key Realization**:
- Arduino Servo.h: Timer-based ISR delivers pulses at fixed intervals, relies on mechanical inertia
- ESP32Servo: LEDC hardware PWM outputs continuous duty cycle, no built-in smoothing
- **We may be fighting the library's design** by trying to add software interpolation on top
- The servo is physically capable (TwoSkulls proves it) - the issue is how we're driving it

**Fundamental Question**: 
Are we over-engineering this? The GPT-5 research states "No built-in interpolation/ramping" for ESP32Servo - meaning we're implementing interpolation that the library doesn't support. Maybe we need to:
1. Stop trying to smooth in software
2. Let the servo respond naturally to direct position commands
3. Or find a way to use ESP32 LEDC features directly with interpolation/smoothing

**Next Steps**: Revert interpolation entirely, try setting target position directly and letting servo respond naturally. If that doesn't work, investigate using ESP32 LEDC API directly with ramp/duty curve features.

### Step 8 (20250123-UNKNOWN): Direct Position Setting Failed - Reverting to Best Working State
**Action**: Tried removing all interpolation, setting target position directly.

**Result**: 
- Way worse - jerking open and closed instantly
- Confirmed that interpolation IS necessary
- Need to revert to best-known working implementation

**Conclusion**: Best result was with 20ms delays and linear interpolation (before timer width changes). Reverting to that implementation.

**Next Steps**: Upload reverted code with 20ms delays, keep 20-bit timer width setting as it doesn't hurt. Accept that we may not achieve Arduino Servo.h levels of smoothness with ESP32Servo library due to fundamental architectural differences.

---

## Validation Summary

### Changes Made
1. **Story documentation**: Added story 001b to `docs/stories.md` and created `docs/stories/story-001b-breathing-cycle.md`
2. **Main program**: Added breathing cycle implementation to `src/main.cpp`:
   - Global constants (BREATHING_INTERVAL, BREATHING_JAW_ANGLE, BREATHING_MOVEMENT_DURATION)
   - `breathingJawMovement()` function
   - Loop integration for timing control
3. **Servo controller**: Modified `src/servo_controller.cpp`:
   - Added `servo.setTimerWidth(20)` for increased PWM resolution
   - Modified `smoothMove()` with clamped progress and clearer comments

### Functional Assessment
- **Breathing cycle timing**: ✅ Working correctly (7-second intervals)
- **Audio check**: ✅ Only operates when no audio playing
- **Servo motion**: ⚠️ Functional but exhibits visible stepping/jerky motion
- **Code quality**: ✅ Clean implementation, follows project patterns
- **Integration**: ✅ Properly integrated with existing state machine

### Current Limitations
- **Motion smoothness**: Does not match TwoSkulls implementation quality
- **Known issue**: ESP32Servo library appears to have fundamental differences from Arduino Servo.h that prevent achieving the same smoothness
- **Workarounds attempted**: Timer width increase, microsecond precision, update frequency changes - none fully resolved the stepping

### Recommended Next Steps
1. Accept current implementation as "best effort" with ESP32Servo
2. Consider alternative approach: Use Arduino IDE for servo control if smoothness is critical
3. Investigate alternative servo libraries that may provide better smoothing features
4. Consider hardware solutions: Different servo, servo controller board with built-in interpolation

**Status**: ✅ Implemented with limitations, ready for user acceptance testing