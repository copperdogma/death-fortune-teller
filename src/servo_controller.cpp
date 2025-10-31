#include "servo_controller.h"
#include "logging_manager.h"
#include "config_manager.h"
#include <algorithm>
#include <cmath>

static constexpr const char* TAG = "Servo";

ServoController::ServoController()
    : servoPin(-1),
      currentPosition(0),
      minDegrees(0),
      maxDegrees(0),
      minMicroseconds(1400),
      maxMicroseconds(1600),
      reverseDirection(false),
      smoothedPosition(0),
      lastPosition(0),
      maxObservedRMS(0),
      shouldInterruptMovement(false)
{
}

void ServoController::initialize(int pin, int minDeg, int maxDeg)
{
    servoPin = pin;

    ConfigManager &config = ConfigManager::getInstance();
    minMicroseconds = config.getServoUSMin();
    maxMicroseconds = config.getServoUSMax();
    reverseDirection = config.getServoReverse();

    servo.attach(servoPin,
                Servo::CHANNEL_NOT_ATTACHED,
                Servo::DEFAULT_MIN_ANGLE,
                Servo::DEFAULT_MAX_ANGLE,
                minMicroseconds,
                maxMicroseconds,
                50);

    setMinMaxDegrees(minDeg, maxDeg);
    setPosition(minDeg); // Start at minimum

    LOG_INFO(TAG,
             "Initializing servo on pin %d (degrees: %d-%d, microseconds: %d-%d)",
             servoPin,
             minDegrees,
             maxDegrees,
             minMicroseconds,
             maxMicroseconds);

    // Perform full sweep animation using smoothMove for proper timing
    LOG_DEBUG(TAG, "Servo animation init: moving to max position (%d degrees)", maxDegrees);
    smoothMove(maxDegrees, 1500); // 1.5 seconds to move to max
    delay(200); // Brief pause at max position
    
    LOG_DEBUG(TAG, "Servo animation init: moving to min position (%d degrees)", minDegrees);
    smoothMove(minDegrees, 1500); // 1.5 seconds to move back to min
    LOG_INFO(TAG, "Servo animation init complete");
}

void ServoController::initialize(int pin, int minDeg, int maxDeg, int minUs, int maxUs)
{
    servoPin = pin;
    minMicroseconds = minUs;
    maxMicroseconds = maxUs;

    ConfigManager &config = ConfigManager::getInstance();
    reverseDirection = config.getServoReverse();

    servo.attach(servoPin,
                Servo::CHANNEL_NOT_ATTACHED,
                Servo::DEFAULT_MIN_ANGLE,
                Servo::DEFAULT_MAX_ANGLE,
                minMicroseconds,
                maxMicroseconds,
                50);

    setMinMaxDegrees(minDeg, maxDeg);
    setPosition(minDeg); // Start at minimum

    LOG_INFO(TAG,
             "Initializing servo on pin %d (degrees: %d-%d, microseconds: %d-%d)",
             servoPin,
             minDegrees,
             maxDegrees,
             minMicroseconds,
             maxMicroseconds);

    // Perform full sweep animation using smoothMove for proper timing
    LOG_DEBUG(TAG, "Servo animation init: moving to max position (%d degrees)", maxDegrees);
    smoothMove(maxDegrees, 1500); // 1.5 seconds to move to max
    delay(200); // Brief pause at max position
    
    LOG_DEBUG(TAG, "Servo animation init: moving to min position (%d degrees)", minDegrees);
    smoothMove(minDegrees, 1500); // 1.5 seconds to move back to min
    LOG_INFO(TAG, "Servo animation init complete");
}

void ServoController::setPosition(int degrees)
{
    int constrainedDegrees = constrain(degrees, minDegrees, maxDegrees);
    int angleToSend = constrainedDegrees;
    
    // Apply direction reversal if enabled (invert the angle before sending to servo)
    if (reverseDirection) {
        angleToSend = 180 - angleToSend;
    }
    
    servo.write(angleToSend);

    // LOG_DEBUG(TAG,
    //           "setPosition(%d°) within [%d,%d]° (µs %d-%d)",
    //           angleToSend,
    //           minDegrees,
    //           maxDegrees,
    //           minMicroseconds,
    //           maxMicroseconds);

    // Track the original (non-inverted) position for our own bookkeeping
    currentPosition = constrainedDegrees;
}

void ServoController::writeMicroseconds(int microseconds)
{
    int constrainedUs = constrain(microseconds, minMicroseconds, maxMicroseconds);
    servo.writeMicroseconds(constrainedUs);
}

int ServoController::getPosition() const
{
    return currentPosition;
}

void ServoController::setMinMaxDegrees(int minDeg, int maxDeg)
{
    minDegrees = minDeg;
    maxDegrees = maxDeg;
}

int ServoController::mapRMSToPosition(double rms, double silenceThreshold)
{
    if (rms < silenceThreshold)
    {
        return minDegrees;
    }

    if (rms > maxObservedRMS)
    {
        maxObservedRMS = rms;
    }

    double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);
    double mappedValue = pow(normalizedRMS, 0.2); // 0.2 is the MOVE_EXPONENT

    int minJawOpening = minDegrees + 5;
    return map(mappedValue * 1000, 0, 1000, minJawOpening, maxDegrees);
}

void ServoController::updatePosition(int targetPosition, double alpha, int minMovementThreshold)
{
    double current = smoothedPosition;
    double updated = alpha * targetPosition + (1 - alpha) * current;

    smoothedPosition = updated;

    int newPosition = round(updated);
    newPosition = constrain(newPosition, minDegrees, maxDegrees);

    int last = lastPosition;

    if (abs(newPosition - last) > minMovementThreshold)
    {
        setPosition(newPosition);
        lastPosition = newPosition;
    }
}

void ServoController::smoothMove(int targetPosition, int duration)
{
    int startPosition = currentPosition;
    unsigned long startTime = millis();
    unsigned long endTime = startTime + duration;

    shouldInterruptMovement = false;

    while (millis() < endTime)
    {
        if (shouldInterruptMovement)
        {
            shouldInterruptMovement = false;
            return;
        }

        unsigned long currentTime = millis();
        float progress = static_cast<float>(currentTime - startTime) / duration;

        if (progress < 0.0f)
        {
            progress = 0.0f;
        }
        else if (progress > 1.0f)
        {
            progress = 1.0f;
        }

        int newPosition = startPosition + (targetPosition - startPosition) * progress;

        setPosition(newPosition);
        delay(20);
    }

    setPosition(targetPosition);
}

void ServoController::interruptMovement()
{
    shouldInterruptMovement = true;
}

void ServoController::reattachWithConfigLimits()
{
    ConfigManager &config = ConfigManager::getInstance();
    minMicroseconds = config.getServoUSMin();
    maxMicroseconds = config.getServoUSMax();
    reverseDirection = config.getServoReverse();

    servo.detach();
    servo.attach(servoPin,
                Servo::CHANNEL_NOT_ATTACHED,
                Servo::DEFAULT_MIN_ANGLE,
                Servo::DEFAULT_MAX_ANGLE,
                minMicroseconds,
                maxMicroseconds,
                50);

    LOG_INFO(TAG,
             "Servo reattached with config limits: %d-%d µs (degrees %d-%d)",
             minMicroseconds,
             maxMicroseconds,
             minDegrees,
             maxDegrees);

    // Reset position tracking after reattach (servo may be at unknown position)
    currentPosition = minDegrees;
    setPosition(minDegrees);
    delay(100); // Brief pause to ensure servo is at known position

    // Perform full sweep animation using smoothMove for proper timing
    LOG_DEBUG(TAG, "Servo config animation: moving to max position (%d degrees)", maxDegrees);
    smoothMove(maxDegrees, 1500); // 1.5 seconds to move to max
    delay(200); // Brief pause at max position
    
    LOG_DEBUG(TAG, "Servo config animation: moving to min position (%d degrees)", minDegrees);
    smoothMove(minDegrees, 1500); // 1.5 seconds to move back to min
    LOG_INFO(TAG, "Servo config animation complete");
}
