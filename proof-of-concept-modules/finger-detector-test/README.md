# ESP32-S3 Capacitive Touch Sensor POC

**Status: ✅ WORKING**

A proof-of-concept for detecting finger proximity using ESP32-S3's capacitive touch sensors with external copper electrodes.

---

## 🎯 Key Learnings

### 1. ESP32-S3 Touch Sensors Work OPPOSITE to ESP32
- **ESP32 (original)**: Touch values **decrease** when touched
- **ESP32-S3**: Touch values **INCREASE** when touched
- **Impact**: Detection logic must check for increases, not decreases

### 2. Sensitivity Must Be Reduced for External Electrodes
```cpp
// ❌ WRONG - Causes saturation with external electrodes
touchSetCycles(0x7FFF, 0x7FFF);  // Max sensitivity

// ✅ CORRECT - Prevents saturation
touchSetCycles(0x1000, 0x1000);  // ~1/8th sensitivity
```

**Saturation symptoms:**
- Touch values stuck at ~4,140,870 (max value)
- Sensor never recovers after first touch
- No variance in readings

### 3. Detection Threshold is VERY Low
- **Expected**: ~3% (0.03)
- **Actual**: ~0.2% (0.002) for copper sheet electrodes
- **Reason**: Large electrodes create small relative changes
- **Solution**: Runtime-tunable thresholds via CLI

### 4. USB CDC On Boot Required
- **Problem**: ESP32-S3 has two USB ports (USB-OTG and USB-UART)
- **Without CDC**: Must switch cables between programming and serial monitoring
- **Solution**: Enable "USB CDC On Boot" in Arduino IDE
  - Tools → USB CDC On Boot → **Enabled**
  - Now USB-OTG port handles both programming AND serial

---

## 🔧 Hardware Setup

### Board: Freenove ESP32-S3-WROOM

**Touch Pins Available:** GPIO1-GPIO14 (T1-T14)
- This POC uses **GPIO2 (T2)** connected to copper sheet electrode

**USB Ports:**
- **USB-OTG**: Programming + Serial (with CDC enabled)
- **USB-UART**: Backup serial option

**Arduino IDE Settings:**
- Board: "ESP32S3 Dev Module"
- USB CDC On Boot: **Enabled** ⚠️ (Critical!)
- Upload Speed: 921600
- USB Mode: "Hardware CDC and JTAG"

---

## 📊 How Touch Detection Works

### Basic Algorithm
```cpp
// 1. Calibrate baseline when finger is NOT present
float baseline = average_of_100_readings();

// 2. Continuously read and filter sensor
int raw = touchRead(TOUCH_PIN);
float filtered = 0.7*filtered + 0.3*raw;  // Low-pass filter

// 3. Calculate percentage change
float delta = (filtered - baseline) / baseline;

// 4. Detect when delta exceeds threshold
bool detected = (delta > THRESHOLD);  // e.g., 0.002 = 0.2%

// 5. Adaptive baseline (slow drift compensation)
baseline = 0.9999*baseline + 0.0001*filtered;
```

### Why Adaptive Baseline?
- Temperature changes affect capacitance
- Humidity changes affect readings
- Slow drift (0.01% per sample) keeps baseline current without following finger movements

### Debouncing
- Detection must be sustained for 30ms (default) to confirm touch
- Prevents false triggers from electrical noise
- Tunable via CLI: `debounce <ms>`

---

## 🎮 CLI Commands

The sketch includes a serial command interface for runtime tuning:

```
help              - Show all commands
cal               - Recalibrate baseline (remove finger first!)
thresh <val>      - Set threshold (0.0-1.0, e.g., 0.002 = 0.2%)
debounce <ms>     - Set debounce time (default: 30ms)
interval <ms>     - Set print interval (default: 500ms)
print on|off      - Toggle continuous printing
status            - Show current sensor values
settings          - Show all current settings
```

### Example Usage
```
>>> settings
Threshold:       0.2%
Debounce:        30 ms
Print Interval:  500 ms
Continuous:      ON
Baseline:        2908655

>>> thresh 0.005
>>> Threshold set to 0.5%

>>> cal
>>> Calibrating baseline (1 second)...
>>> REMOVE YOUR FINGER NOW!
>>> New baseline: 2908103
>>> Calibration complete!
```

---

## ⚡ Quick Start

1. **Wire electrode** to GPIO2 (or any T1-T14 pin)
2. **Flash sketch** with Arduino IDE
3. **Power on** with finger FAR from sensor
4. **Watch serial output** (115200 baud)
5. **Approach sensor** - see delta increase
6. **Touch sensor** - see "CONFIRMED TOUCH!" message
7. **Tune threshold** if needed: `thresh 0.005`

---

## 🐛 Common Issues

### Sensor Saturates (Stuck at 4,140,870)
**Cause:** Sensitivity too high for large electrode
**Fix:** Already set to `0x1000` in code. If still happening:
```cpp
// Try even lower sensitivity
touchSetCycles(0x800, 0x800);  // Line 22
```

### False Detections at 0.1%
**Cause:** Threshold too low
**Fix:** Increase threshold
```
thresh 0.01  // Set to 1%
```

### No Detection Even When Touching
**Causes:**
1. Threshold too high → Lower it: `thresh 0.001`
2. Bad calibration → Recalibrate: `cal`
3. Finger on sensor during boot → Reset with finger away

### Serial Monitor Not Working
**Cause:** USB CDC On Boot disabled
**Fix:** Tools → USB CDC On Boot → **Enabled**, then re-upload

---

## 📝 Code Structure

```cpp
// Global Parameters (Runtime Tunable)
THRESH_N = 0.002f;     // 0.2% detection threshold
DEBOUNCE = 30;         // 30ms confirmation time
PRINT_INTERVAL = 500;  // Status every 500ms
CONTINUOUS_PRINT = true;

// Setup:
// 1. Configure touch sensor with moderate sensitivity
// 2. Calibrate baseline (1 second, 100 samples)
// 3. Start CLI

// Loop:
// 1. Handle CLI commands
// 2. Read touch sensor
// 3. Check for saturation
// 4. Apply low-pass filter
// 5. Update adaptive baseline
// 6. Calculate delta from baseline
// 7. Check threshold
// 8. Debounce detection
// 9. Print status (if enabled)
```

---

## 🎯 Critical Implementation Details

### 1. Saturation Detection
```cpp
if (raw > 4000000) {
    Serial.println("!!! SENSOR SATURATED !!!");
    delay(1000);
    return;  // Skip this loop iteration
}
```

### 2. Filtering
```cpp
// Exponential moving average (70% old, 30% new)
filt = 0.7f*filt + 0.3f*raw;
```

### 3. Adaptive Baseline
```cpp
// Very slow drift (0.01% per sample @ 100Hz = 10%/second)
base = 0.9999f*base + 0.0001f*filt;
```

### 4. Detection Logic
```cpp
float delta = (filt - base) / (base > 1 ? base : 1);
bool detected = delta > THRESH_N;
```

### 5. Debouncing
```cpp
if (detected) {
    if (!okSince) okSince = now;
    if (now - okSince >= DEBOUNCE) {
        // Confirmed touch!
    }
} else {
    okSince = 0;  // Reset timer
}
```

---

## 📊 Typical Values

### With Copper Sheet Electrode (~10cm²):
- **Baseline**: 2,900,000 - 3,000,000
- **Finger nearby (5cm)**: +0.1% to +0.3%
- **Finger close (1cm)**: +0.3% to +0.8%
- **Touching**: +0.5% to +2.0%
- **Optimal threshold**: 0.2% (0.002)

### Smaller Electrodes:
- Higher baseline values possible
- Larger percentage changes
- Can use higher thresholds (e.g., 3% / 0.03)

---

## 🔬 Tuning Guide

### Start Conservative
```
thresh 0.01  // 1% - unlikely to false trigger
```

### Test and Reduce
1. Approach sensor slowly
2. Note delta percentage when you want detection
3. Set threshold slightly below that value
4. Test with rapid movements to check false triggers

### Fine-Tune Debounce
```
debounce 10   // More responsive, more noise-sensitive
debounce 50   // Less responsive, more stable
```

### Optimize Print Interval
```
print off      // Stop continuous output for clean testing
status         // Check values on demand
print on       // Resume continuous monitoring
```

---

## 🎓 For AI Agents Building Production Projects

### Copy These Patterns:
1. **Reduced sensitivity** - `touchSetCycles(0x1000, 0x1000)` for external electrodes
2. **Saturation detection** - Check for max value (~4,140,870) and handle gracefully
3. **Adaptive baseline** - Slow drift compensation prevents long-term calibration issues
4. **Runtime tuning** - CLI allows field adjustment without recompiling
5. **Direction awareness** - ESP32-S3 values increase (not decrease) on touch

### Don't Copy (POC Only):
- LED code is commented out (GPIO48 needs NeoPixel library)
- Single touch pin (production might need multiple)
- Simple threshold detection (production might need ML/adaptive algorithms)

### Critical Gotchas:
1. ⚠️ **ESP32 vs ESP32-S3**: Touch behavior is OPPOSITE
2. ⚠️ **USB CDC On Boot**: Must enable for convenient development
3. ⚠️ **Calibrate untouched**: Baseline MUST be captured with finger away
4. ⚠️ **Sensitivity too high**: External electrodes need ~1/8th max sensitivity
5. ⚠️ **Very low thresholds**: Large electrodes create small relative changes (0.2% not 3%)

---

## 📚 Datasheets

Located in `datasheets/`:
- `esp32-s3_datasheet_en.pdf` - Chip specifications
- `esp32-s3_technical_reference_manual_en.pdf` - Touch sensor details
- `esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf` - Module specifications
- `ESP32S3_Pinout.png` - Freenove board pinout with touch pins highlighted

---

## 🚀 Next Steps for Production

1. **Multiple sensors** - Detect which area is touched
2. **Gesture recognition** - Swipe, tap, hold patterns
3. **Auto-calibration** - Periodic baseline updates
4. **Power optimization** - Sleep modes between readings
5. **Noise filtering** - Advanced DSP for electrically noisy environments
6. **Temperature compensation** - Adjust threshold based on temp sensor

---

## ✅ POC Success Criteria: ACHIEVED

- [x] Capacitive touch detection working with external electrode
- [x] Reliable detection with copper sheet (~10cm²)
- [x] Saturation prevention for large electrodes
- [x] Runtime tuning via CLI
- [x] Adaptive baseline drift compensation
- [x] USB CDC configuration documented
- [x] ESP32 vs ESP32-S3 differences documented
- [x] Production-ready detection algorithm

**Status: COMPLETE** ✅

---

*Last Updated: October 14, 2025*
*Board: Freenove ESP32-S3-WROOM*
*Arduino IDE 2.x with ESP32 core*

