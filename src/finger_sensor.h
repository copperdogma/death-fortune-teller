#ifndef FINGER_SENSOR_H
#define FINGER_SENSOR_H

#include <Arduino.h>

class FingerSensor {
public:
    FingerSensor(int pin);
    void begin();
    void update();
    bool isFingerDetected();
    void calibrate();
    int getRawValue();
    int getThreshold();

private:
    int pin;
    int threshold;
    int baseline;
    int rawValue;
    bool fingerDetected;
    unsigned long detectionStartTime;
    unsigned long lastDetectionTime;
    
    static constexpr int DETECTION_THRESHOLD = 50;  // Adjust based on testing
    static constexpr unsigned long DETECTION_DURATION_MS = 120;  // 120ms solid detection required
    static constexpr unsigned long CALIBRATION_TIME_MS = 2000;   // 2 seconds for calibration
    static constexpr unsigned long UPDATE_INTERVAL_MS = 10;     // Update every 10ms
    
    unsigned long lastUpdateTime;
    bool isCalibrated;
    
    void performCalibration();
    void updateDetection();
};

#endif // FINGER_SENSOR_H

