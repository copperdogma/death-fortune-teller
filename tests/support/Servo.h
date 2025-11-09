#ifndef SERVO_STUB_H
#define SERVO_STUB_H

#include <cstdint>

class Servo {
public:
    static constexpr int CHANNEL_NOT_ATTACHED = -1;
    static constexpr int DEFAULT_MIN_ANGLE = 0;
    static constexpr int DEFAULT_MAX_ANGLE = 180;

    Servo() = default;

    void setPeriodHertz(int) {}

    bool attach(int pin, int channel = CHANNEL_NOT_ATTACHED, int minUs = 544, int maxUs = 2400) {
        (void)pin;
        (void)channel;
        minMicroseconds = minUs;
        maxMicroseconds = maxUs;
        attachedFlag = true;
        return true;
    }

    void detach() { attachedFlag = false; }

    bool attached() const { return attachedFlag; }

    void write(int angle) { lastAngle = angle; }
    void writeMicroseconds(int microseconds) { lastMicroseconds = microseconds; }
    int read() const { return lastAngle; }

    int lastAngle = 0;
    int lastMicroseconds = 0;
    int minMicroseconds = 544;
    int maxMicroseconds = 2400;

private:
    bool attachedFlag = false;
};

#endif  // SERVO_STUB_H

