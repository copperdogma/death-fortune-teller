#ifndef SERVO_CONTROLLER_STUB_H
#define SERVO_CONTROLLER_STUB_H

#include <Arduino.h>
#include <vector>

class ServoController {
public:
    ServoController()
        : currentPosition(-1),
          minDegrees(0),
          maxDegrees(80),
          minMicroseconds(1500),
          maxMicroseconds(1500),
          reversed(false) {}

    void setInitialState(int position, int minDeg, int maxDeg, int minUs, int maxUs) {
        currentPosition = position;
        minDegrees = minDeg;
        maxDegrees = maxDeg;
        minMicroseconds = minUs;
        maxMicroseconds = maxUs;
    }

    int getPosition() const { return currentPosition; }
    int getMinDegrees() const { return minDegrees; }
    int getMaxDegrees() const { return maxDegrees; }
    int getMinMicroseconds() const { return minMicroseconds; }
    int getMaxMicroseconds() const { return maxMicroseconds; }
    bool isReversed() const { return reversed; }

    void reattachWithConfigLimits() { reattachCalls++; }

    void smoothMove(int targetPosition, int duration) {
        lastSmoothMoveTarget = targetPosition;
        lastSmoothMoveDuration = duration;
        currentPosition = targetPosition;
    }

    void setMinMicroseconds(int us) {
        minMicroseconds = constrain(us, 500, 10000);
        minMicrosecondsUpdates.push_back(minMicroseconds);
    }

    void setMaxMicroseconds(int us) {
        maxMicroseconds = constrain(us, 500, 10000);
        maxMicrosecondsUpdates.push_back(maxMicroseconds);
    }

    void writeMicroseconds(int us) {
        lastWrittenMicros = us;
    }

    void setPosition(int degrees) {
        currentPosition = degrees;
        lastSetPosition = degrees;
    }

    void setReverseDirection(bool reverse) { reversed = reverse; }

    // Tracking helpers for tests
    int reattachCalls = 0;
    int lastSmoothMoveTarget = -1;
    int lastSmoothMoveDuration = 0;
    std::vector<int> minMicrosecondsUpdates;
    std::vector<int> maxMicrosecondsUpdates;
    int lastWrittenMicros = 0;
    int lastSetPosition = -1;

private:
    int currentPosition;
    int minDegrees;
    int maxDegrees;
    int minMicroseconds;
    int maxMicroseconds;
    bool reversed;
};

#endif  // SERVO_CONTROLLER_STUB_H
