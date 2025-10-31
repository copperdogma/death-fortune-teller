#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include <Servo.h>

class ServoController {
private:
    Servo servo;
    int servoPin;
    int currentPosition;
    int minDegrees;
    int maxDegrees;
    int minMicroseconds;  // Hard limits - NEVER exceed these
    int maxMicroseconds;  // Hard limits - NEVER exceed these
    bool reverseDirection;  // If true, invert the servo direction
    double smoothedPosition;
    int lastPosition;
    double maxObservedRMS;
    bool shouldInterruptMovement;  // Renamed from interruptMovement

public:
    ServoController();
    void initialize(int pin, int minDeg, int maxDeg);
    void initialize(int pin, int minDeg, int maxDeg, int minUs, int maxUs);
    void setPosition(int degrees);
    void writeMicroseconds(int microseconds);
    int getPosition() const;
    void setMinMaxDegrees(int minDeg, int maxDeg);
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);
    void smoothMove(int targetPosition, int duration);
    void interruptMovement();
    
    // Re-attach servo with config limits (call after config loads)
    void reattachWithConfigLimits();
    
    // Set servo direction reversal (invert the mapping)
    void setReverseDirection(bool reverse) { reverseDirection = reverse; }

    int getMinDegrees() const { return minDegrees; }
    int getMaxDegrees() const { return maxDegrees; }
    int getMinMicroseconds() const { return minMicroseconds; }
    int getMaxMicroseconds() const { return maxMicroseconds; }
    bool isReversed() const { return reverseDirection; }
    
    // Setters for tuning min/max microseconds
    void setMinMicroseconds(int us) { minMicroseconds = constrain(us, 500, 10000); }
    void setMaxMicroseconds(int us) { maxMicroseconds = constrain(us, 500, 10000); }
};

#endif // SERVO_CONTROLLER_H
