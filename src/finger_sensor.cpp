#include "finger_sensor.h"
#include "logging_manager.h"

#include <algorithm>
#include <cmath>

static constexpr const char *TAG = "FingerSensor";

// Static parameter defaults
uint16_t FingerSensor::s_touchCyclesInitial = 0x1000;
uint16_t FingerSensor::s_touchCyclesMeasure = 0x1000;
float FingerSensor::s_filterAlpha = FingerSensor::FILTER_ALPHA_DEFAULT;
float FingerSensor::s_baselineDrift = 0.0001f;
uint8_t FingerSensor::s_multisampleCount = 32;

FingerSensor::FingerSensor(int pin)
    : m_pin(pin),
      m_thresholdRatio(0.002f),
      m_stableDurationMs(120),
      m_rawValue(0.0f),
      m_lastRawSample(0.0f),
      m_lastAverageSample(0.0f),
      m_baseline(0.0f),
      m_filtered(0.0f),
      m_normalizedDelta(0.0f),
      m_isCalibrated(false),
      m_isCalibrating(false),
      m_calibrationStartMs(0),
      m_calibrationSamples(0),
      m_calibrationSum(0.0),
      m_touchActive(false),
      m_stableTouch(false),
      m_detectionStartMs(0),
      m_lastUpdateMs(0),
      m_streamEnabled(false),
      m_streamIntervalMs(500),
      m_lastStreamPrintMs(0) {}

void FingerSensor::begin()
{
    touchSetCycles(s_touchCyclesInitial, s_touchCyclesMeasure);
    startCalibration(true);
}

void FingerSensor::update()
{
    unsigned long currentTime = millis();
    if (currentTime - m_lastUpdateMs < UPDATE_INTERVAL_MS) {
        return;
    }
    m_lastUpdateMs = currentTime;

    if (!m_isCalibrated) {
        performCalibration();
        return;
    }

    float sample = readTouchAverage();
    m_lastRawSample = sample;
    m_rawValue = sample;

    if (!m_isCalibrated) {
        return;
    }

    if (m_calibrationSamples == 0 && m_filtered == 0.0f) {
        m_filtered = sample;
    }

    float alpha = std::clamp(s_filterAlpha, 0.0f, 1.0f);
    m_filtered = alpha * sample + (1.0f - alpha) * m_filtered;

    if (!m_touchActive) {
        float drift = std::clamp(s_baselineDrift, 0.0f, 0.1f);
        m_baseline = drift * m_filtered + (1.0f - drift) * m_baseline;
    }

    float denominator = m_baseline > 1.0f ? m_baseline : 1.0f;
    m_normalizedDelta = (m_baseline - m_filtered) / denominator;
    if (m_normalizedDelta < 0.0f) {
        m_normalizedDelta = 0.0f;
    }

    bool currentlyDetected = m_normalizedDelta >= m_thresholdRatio;
    updateDetection(currentTime, m_normalizedDelta, currentlyDetected);

    if (m_streamEnabled) {
        if (currentTime - m_lastStreamPrintMs >= m_streamIntervalMs) {
            m_lastStreamPrintMs = currentTime;
            printStreamSample();
        }
    }
}

void FingerSensor::calibrate()
{
    startCalibration(true);
}

bool FingerSensor::isFingerDetected() const
{
    return m_touchActive;
}

bool FingerSensor::hasStableTouch() const
{
    return m_stableTouch;
}

float FingerSensor::getRawValue() const
{
    return m_rawValue;
}

float FingerSensor::getBaseline() const
{
    return m_baseline;
}

float FingerSensor::getFilteredValue() const
{
    return m_filtered;
}

float FingerSensor::getNormalizedDelta() const
{
    return m_normalizedDelta;
}

float FingerSensor::getThresholdRatio() const
{
    return m_thresholdRatio;
}

unsigned long FingerSensor::getStableDurationMs() const
{
    return m_stableDurationMs;
}

unsigned long FingerSensor::getStreamIntervalMs() const
{
    return m_streamIntervalMs;
}

bool FingerSensor::setThresholdRatio(float ratio)
{
    if (ratio < MIN_THRESHOLD_RATIO || ratio > 1.0f) {
        return false;
    }
    m_thresholdRatio = ratio;
    LOG_INFO(TAG, "Threshold updated to %.3f%%", m_thresholdRatio * 100.0f);
    return true;
}

bool FingerSensor::setStableDurationMs(unsigned long durationMs)
{
    if (durationMs < 30 || durationMs > 1000) {
        return false;
    }
    m_stableDurationMs = durationMs;
    LOG_INFO(TAG, "Stable touch duration set to %lums", m_stableDurationMs);
    return true;
}

void FingerSensor::setStreamEnabled(bool enabled)
{
    m_streamEnabled = enabled;
    m_lastStreamPrintMs = 0;
    LOG_INFO(TAG, "Finger sensor stream %s", enabled ? "enabled" : "disabled");
}

bool FingerSensor::isStreamEnabled() const
{
    return m_streamEnabled;
}

bool FingerSensor::setStreamIntervalMs(unsigned long intervalMs)
{
    if (intervalMs < 100 || intervalMs > 10000) {
        return false;
    }
    m_streamIntervalMs = intervalMs;
    LOG_INFO(TAG, "Stream interval set to %lums", m_streamIntervalMs);
    return true;
}

bool FingerSensor::setTouchCycles(uint16_t initialCycles, uint16_t measureCycles)
{
    if (initialCycles == 0 || measureCycles == 0) {
        return false;
    }
    s_touchCyclesInitial = initialCycles;
    s_touchCyclesMeasure = measureCycles;
    touchSetCycles(s_touchCyclesInitial, s_touchCyclesMeasure);
    LOG_INFO(TAG, "Touch cycles updated: init=0x%04X measure=0x%04X", s_touchCyclesInitial, s_touchCyclesMeasure);
    return true;
}

bool FingerSensor::setFilterAlpha(float alpha)
{
    if (alpha < 0.0f || alpha > 1.0f) {
        return false;
    }
    s_filterAlpha = alpha;
    LOG_INFO(TAG, "Filter alpha set to %.4f", s_filterAlpha);
    return true;
}

bool FingerSensor::setBaselineDrift(float drift)
{
    if (drift < 0.0f || drift > 0.1f) {
        return false;
    }
    s_baselineDrift = drift;
    LOG_INFO(TAG, "Baseline drift set to %.6f", s_baselineDrift);
    return true;
}

bool FingerSensor::setMultisampleCount(uint8_t count)
{
    if (count == 0) {
        return false;
    }
    s_multisampleCount = count;
    LOG_INFO(TAG, "Multisample count set to %u", s_multisampleCount);
    return true;
}

void FingerSensor::printStatus(Print &out) const
{
    float absoluteDelta = std::max(0.0f, m_baseline - m_filtered);
    float thresholdAbsolute = m_thresholdRatio * (m_baseline > 1.0f ? m_baseline : 1.0f);

    out.println("\n=== FINGER SENSOR STATUS ===");
    out.print("raw:             "); out.println(m_rawValue, 3);
    out.print("filtered:        "); out.println(m_filtered, 3);
    out.print("baseline:        "); out.println(m_baseline, 3);
    out.print("delta (raw):     "); out.println(absoluteDelta, 2);
    out.print("threshold (raw): "); out.println(thresholdAbsolute, 2);
    out.print("delta (norm):    "); out.println(m_normalizedDelta, 4);
    out.print("threshold (norm):"); out.println(m_thresholdRatio, 4);
    out.print("stable duration: "); out.print(m_stableDurationMs); out.println(" ms");
    out.print("touchSetCycles:  "); out.print(s_touchCyclesInitial, HEX); out.print(" / "); out.println(s_touchCyclesMeasure, HEX);
    out.print("alpha:           "); out.println(s_filterAlpha, 4);
    out.print("baseline drift:  "); out.println(s_baselineDrift, 6);
    out.print("multisample N:   "); out.println(s_multisampleCount);
    out.print("stream:          "); out.println(m_streamEnabled ? "ON" : "OFF");
    out.print("stream interval: "); out.print(m_streamIntervalMs); out.println(" ms");
    out.print("touch active:    "); out.println(m_touchActive ? "YES" : "NO");
    out.print("touch stable:    "); out.println(m_stableTouch ? "YES" : "NO");
    out.println();
}

void FingerSensor::printSettings(Print &out) const
{
    out.println("\n=== FINGER SENSOR SETTINGS ===");
    out.print("threshold (norm): "); out.println(m_thresholdRatio, 4);
    out.print("stable duration:  "); out.print(m_stableDurationMs); out.println(" ms");
    out.print("stream interval:  "); out.print(m_streamIntervalMs); out.println(" ms");
    out.print("touchSetCycles:   "); out.print(s_touchCyclesInitial, HEX); out.print(" / "); out.println(s_touchCyclesMeasure, HEX);
    out.print("alpha:            "); out.println(s_filterAlpha, 4);
    out.print("baseline drift:   "); out.println(s_baselineDrift, 6);
    out.print("multisample N:    "); out.println(s_multisampleCount);
    out.print("stream:           "); out.println(m_streamEnabled ? "ON" : "OFF");
    out.println();
}

void FingerSensor::startCalibration(bool logMessage)
{
    m_isCalibrated = false;
    m_isCalibrating = true;
    m_calibrationStartMs = millis();
    m_calibrationSamples = 0;
    m_calibrationSum = 0.0;
    m_touchActive = false;
    m_stableTouch = false;
    m_detectionStartMs = 0;
    m_lastStreamPrintMs = 0;

    if (logMessage) {
        LOG_INFO(TAG, "Starting finger sensor calibration... keep the mouth clear.");
    }
}

void FingerSensor::performCalibration()
{
    if (!m_isCalibrating) {
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - m_calibrationStartMs;

    float sample = readTouchAverage();
    m_calibrationSum += sample;
    ++m_calibrationSamples;

    if (elapsed < CALIBRATION_TIME_MS) {
        if (m_calibrationSamples % 25 == 0) {
            LOG_DEBUG(TAG, "Calibration sampling (%u samples)", m_calibrationSamples);
        }
        return;
    }

    if (m_calibrationSamples == 0) {
        LOG_ERROR(TAG, "Calibration failed — no samples collected.");
        startCalibration(false);
        return;
    }

    m_baseline = static_cast<float>(m_calibrationSum / m_calibrationSamples);
    m_filtered = m_baseline;
    m_normalizedDelta = 0.0f;
    m_isCalibrated = true;
    m_isCalibrating = false;
    m_calibrationSamples = 0;
    m_calibrationSum = 0.0;

    LOG_INFO(TAG, "Finger sensor calibrated — baseline=%.0f threshold=%.3f%% stable=%lums",
             m_baseline,
             m_thresholdRatio * 100.0f,
             m_stableDurationMs);
}

void FingerSensor::updateDetection(unsigned long currentTime, float normalizedDelta, bool currentlyDetected)
{
    if (currentlyDetected) {
        if (!m_touchActive) {
            m_touchActive = true;
            m_detectionStartMs = currentTime;
            LOG_INFO(TAG, "Finger touch detected (Δ=%.3f%%)", normalizedDelta * 100.0f);
        }

        if (!m_stableTouch && (currentTime - m_detectionStartMs) >= m_stableDurationMs) {
            m_stableTouch = true;
            LOG_INFO(TAG, "Finger touch stabilized after %lums", currentTime - m_detectionStartMs);
        }
    } else {
        if (m_touchActive) {
            LOG_INFO(TAG, "Finger removed (Δ=%.3f%%)", normalizedDelta * 100.0f);
        }
        m_touchActive = false;
        m_stableTouch = false;
        m_detectionStartMs = 0;
    }
}

void FingerSensor::printStreamSample()
{
    float absoluteDelta = std::max(0.0f, m_baseline - m_filtered);
    float thresholdAbsolute = m_thresholdRatio * (m_baseline > 1.0f ? m_baseline : 1.0f);

    Serial.print("Touch: ");
    Serial.print(m_rawValue, 3);
    Serial.print(" | filtered: ");
    Serial.print(m_filtered, 3);
    Serial.print(" | baseline: ");
    Serial.print(m_baseline, 3);
    Serial.print(" | Δnorm: ");
    Serial.print(m_normalizedDelta, 4);
    Serial.print(" | thresh: ");
    Serial.print(m_thresholdRatio, 4);
    Serial.print(" | Δraw: ");
    Serial.print(absoluteDelta, 3);
    Serial.print(" | thresh_raw: ");
    Serial.print(thresholdAbsolute, 3);
    if (m_touchActive) {
        Serial.print(" <<< DETECTED");
    }
    Serial.println();
}

float FingerSensor::readTouchAverage() const
{
    uint32_t sum = 0;
    uint8_t samples = std::max<uint8_t>(1, s_multisampleCount);
    for (uint8_t i = 0; i < samples; ++i) {
        sum += touchRead(m_pin);
        if (samples > 1) {
            delayMicroseconds(50);
        }
    }
    float average = static_cast<float>(sum) / static_cast<float>(samples);
    return average;
}
