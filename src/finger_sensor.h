#ifndef FINGER_SENSOR_H
#define FINGER_SENSOR_H

#include <Arduino.h>
#include <Print.h>

class FingerSensor {
public:
    explicit FingerSensor(int pin);

    void begin();
    void update();
    void calibrate();

    bool isFingerDetected() const;      // Immediate threshold detection
    bool hasStableTouch() const;        // Detection sustained for configured duration

    float getRawValue() const;
    float getBaseline() const;
    float getFilteredValue() const;
    float getNormalizedDelta() const;
    float getThresholdRatio() const;
    float getSensitivity() const;
    unsigned long getStableDurationMs() const;
    unsigned long getStreamIntervalMs() const;
    float getNoiseNormalized() const;
    float getNoiseAbsolute() const;

    bool setThresholdRatio(float ratio);
    bool setStableDurationMs(unsigned long durationMs);
    void setStreamEnabled(bool enabled);
    bool isStreamEnabled() const;
    bool setStreamIntervalMs(unsigned long intervalMs);
    bool setSensitivity(float sensitivity);

    bool setTouchCycles(uint16_t initialCycles, uint16_t measureCycles);
    bool setFilterAlpha(float alpha);
    bool setBaselineDrift(float drift);
    bool setMultisampleCount(uint8_t count);

    void printStatus(Print &out) const;
    void printSettings(Print &out) const;

private:
    void startCalibration(bool logMessage);
    void performCalibration();
    void updateDetection(unsigned long currentTime, float normalizedDelta, bool currentlyDetected);
    void printStreamSample();

    int m_pin;

    float m_thresholdRatio;          // Normalized delta threshold (e.g., 0.002 = 0.2 %)
    unsigned long m_stableDurationMs;

    float m_rawValue;
    float m_lastRawSample;
    float m_lastAverageSample;
    float m_baseline;
    float m_filtered;
    float m_normalizedDelta;
    float m_noiseAbsolute;
    float m_noiseNormalized;
    float m_sensitivity;
    float m_manualMinThreshold;

    bool m_isCalibrated;
    bool m_isCalibrating;
    unsigned long m_calibrationStartMs;
    uint16_t m_calibrationSamples;
    double m_calibrationSum;
    float m_calibrationMinSample;
    float m_calibrationMaxSample;
    unsigned long m_detectionEnableTime;
    bool m_isSettling;
    unsigned long m_settleEndTime;

    bool m_touchActive;
    bool m_stableTouch;
    unsigned long m_detectionStartMs;
    unsigned long m_lastUpdateMs;
    bool m_streamEnabled;
    unsigned long m_streamIntervalMs;
    unsigned long m_lastStreamPrintMs;

    static constexpr unsigned long CALIBRATION_TIME_MS = 1000; // Gather samples for 1 s
    static constexpr unsigned long UPDATE_INTERVAL_MS = 10;    // Update every 10 ms
    static constexpr unsigned long SETTLE_TIME_MS = 2000;      // Ignore detections while environment settles post-calibration
    static constexpr float FILTER_ALPHA_DEFAULT = 0.3f;     // Default smoothing coefficient
    static constexpr float MIN_THRESHOLD_RATIO = 0.0001f;   // 0.01 %
    static constexpr float MAX_THRESHOLD_RATIO = 0.05f;     // 5 %
    static constexpr float MIN_SENSITIVITY = 0.0f;
    static constexpr float MAX_SENSITIVITY = 1.0f;
    static constexpr float DEFAULT_SENSITIVITY = 0.15f;
    static constexpr float MIN_NOISE_NORMALIZED = 0.001f;   // 0.1 % default noise floor

    static uint16_t s_touchCyclesInitial;   // touchSetCycles initial
    static uint16_t s_touchCyclesMeasure;   // touchSetCycles measure
    static float s_filterAlpha;             // exponential smoothing
    static float s_baselineDrift;           // baseline drift factor
    static uint8_t s_multisampleCount;      // number of samples averaged

    float computeAdaptiveThreshold(float baseline) const;
    void applyManualThresholdClamp();
    float readTouchAverage() const;
};

#endif // FINGER_SENSOR_H
