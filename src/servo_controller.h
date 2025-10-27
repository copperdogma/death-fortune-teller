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
    double smoothedPosition;
    int lastPosition;
    double maxObservedRMS;
    bool shouldInterruptMovement;  // Renamed from interruptMovement

public:
    ServoController();
    void initialize(int pin, int minDeg, int maxDeg);
    void initialize(int pin, int minDeg, int maxDeg, int minUs, int maxUs);
    void setPosition(int degrees);
    int getPosition() const;
    void setMinMaxDegrees(int minDeg, int maxDeg);
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);
    void smoothMove(int targetPosition, int duration);
    void interruptMovement();
    
    // Re-attach servo with config limits (call after config loads)
    void reattachWithConfigLimits();
};

#endif // SERVO_CONTROLLER_H
