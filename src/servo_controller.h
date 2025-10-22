#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include <ESP32Servo.h>

class ServoController {
private:
    Servo servo;
    int servoPin;
    int currentPosition;
    int minDegrees;
    int maxDegrees;
    double smoothedPosition;
    int lastPosition;
    double maxObservedRMS;
    bool shouldInterruptMovement;  // Renamed from interruptMovement

public:
    ServoController();
    void initialize(int pin, int minDeg, int maxDeg);
    void setPosition(int degrees);
    int getPosition() const;
    void setMinMaxDegrees(int minDeg, int maxDeg);
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);
    void smoothMove(int targetPosition, int duration);
    void interruptMovement();
};

#endif // SERVO_CONTROLLER_H