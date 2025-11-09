#ifndef FINGER_SENSOR_STUB_H
#define FINGER_SENSOR_STUB_H

#include <Arduino.h>

class Print {
public:
    virtual ~Print() = default;
};

class FingerSensor {
public:
    FingerSensor() = default;
    explicit FingerSensor(int) {}

    void calibrate() { calibrated = true; }
    bool setSensitivity(float value) { sensitivity = value; return true; }
    float getSensitivity() const { return sensitivity; }
    float getNoiseNormalized() const { return noiseNormalized; }
    bool setThresholdRatio(float value) { threshold = value; return true; }
    float getThresholdRatio() const { return threshold; }
    bool setStableDurationMs(unsigned long value) { stableDurationMs = value; return true; }
    bool setStreamIntervalMs(unsigned long value) { streamIntervalMs = value; return true; }
    void setStreamEnabled(bool value) { streamEnabled = value; }
    bool isStreamEnabled() const { return streamEnabled; }
    bool setTouchCycles(uint16_t initVal, uint16_t measureVal) {
        touchCyclesInit = initVal;
        touchCyclesMeasure = measureVal;
        return initVal > 0 && measureVal > 0;
    }
    bool setFilterAlpha(float value) { filterAlpha = value; return value >= 0.0f && value <= 1.0f; }
    bool setBaselineDrift(float value) { baselineDrift = value; return value >= 0.0f && value <= 0.1f; }
    bool setMultisampleCount(uint8_t count) { multisampleCount = count; return count >= 1; }
    void printStatus(Print &) const { statusPrinted = true; }
    void printSettings(Print &) const { settingsPrinted = true; }
    bool isFingerDetected() const { return fingerDetected; }
    bool hasStableTouch() const { return stableTouch; }
    float getNormalizedDelta() const { return normalizedDelta; }

    bool calibrated = false;
    float sensitivity = 0.5f;
    float noiseNormalized = 0.123f;
    float threshold = 0.01f;
    unsigned long stableDurationMs = 120;
    unsigned long streamIntervalMs = 500;
    bool streamEnabled = false;
    uint16_t touchCyclesInit = 0;
    uint16_t touchCyclesMeasure = 0;
    float filterAlpha = 0.3f;
    float baselineDrift = 0.01f;
    uint8_t multisampleCount = 1;
    mutable bool statusPrinted = false;
    mutable bool settingsPrinted = false;
    bool fingerDetected = false;
    bool stableTouch = false;
    float normalizedDelta = 0.0f;
};

#endif  // FINGER_SENSOR_STUB_H
