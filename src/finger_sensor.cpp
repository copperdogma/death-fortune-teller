#include "finger_sensor.h"

FingerSensor::FingerSensor(int pin) 
    : pin(pin), threshold(DETECTION_THRESHOLD), baseline(0), rawValue(0), 
      fingerDetected(false), detectionStartTime(0), lastDetectionTime(0),
      lastUpdateTime(0), isCalibrated(false) {
}

void FingerSensor::begin() {
    pinMode(pin, INPUT);
    Serial.println("Finger sensor initialized - starting calibration");
    performCalibration();
}

void FingerSensor::update() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastUpdateTime < UPDATE_INTERVAL_MS) {
        return;
    }
    
    lastUpdateTime = currentTime;
    
    if (!isCalibrated) {
        performCalibration();
        return;
    }
    
    // Read raw value
    rawValue = touchRead(pin);
    
    // Update detection state
    updateDetection();
}

bool FingerSensor::isFingerDetected() {
    return fingerDetected;
}

void FingerSensor::calibrate() {
    isCalibrated = false;
    performCalibration();
}

int FingerSensor::getRawValue() {
    return rawValue;
}

int FingerSensor::getThreshold() {
    return threshold;
}

void FingerSensor::performCalibration() {
    static unsigned long calibrationStart = 0;
    static int calibrationSamples = 0;
    static long calibrationSum = 0;
    
    unsigned long currentTime = millis();
    
    if (calibrationStart == 0) {
        calibrationStart = currentTime;
        calibrationSamples = 0;
        calibrationSum = 0;
        Serial.println("Starting finger sensor calibration...");
    }
    
    if (currentTime - calibrationStart < CALIBRATION_TIME_MS) {
        // Collect samples during calibration
        int sample = touchRead(pin);
        calibrationSum += sample;
        calibrationSamples++;
        
        if (calibrationSamples % 100 == 0) {
            Serial.printf("Calibration progress: %d samples\n", calibrationSamples);
        }
    } else {
        // Calibration complete
        baseline = calibrationSum / calibrationSamples;
        threshold = baseline - DETECTION_THRESHOLD;
        
        Serial.printf("Finger sensor calibrated - baseline: %d, threshold: %d\n", baseline, threshold);
        isCalibrated = true;
        
        // Reset calibration variables
        calibrationStart = 0;
        calibrationSamples = 0;
        calibrationSum = 0;
    }
}

void FingerSensor::updateDetection() {
    bool currentlyDetected = (rawValue < threshold);
    
    if (currentlyDetected && !fingerDetected) {
        // Finger just detected
        fingerDetected = true;
        detectionStartTime = millis();
        Serial.println("Finger detected!");
    } else if (!currentlyDetected && fingerDetected) {
        // Finger removed
        fingerDetected = false;
        lastDetectionTime = millis();
        Serial.println("Finger removed");
    }
}

