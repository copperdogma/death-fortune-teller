#include "servo_controller.h"
#include "logging_manager.h"
#include <algorithm>

static constexpr const char* TAG = "Servo";

// ServoController class constructor
// Initializes member variables with default values
ServoController::ServoController()
    : servoPin(-1), currentPosition(0), minDegrees(0), maxDegrees(0),
      smoothedPosition(0), lastPosition(0), maxObservedRMS(0), shouldInterruptMovement(false) {}

// Initialize the servo controller with specified parameters
void ServoController::initialize(int pin, int minDeg, int maxDeg)
{
    // Set member variables with provided values
    servoPin = pin;
    
    // Increase timer width to 20-bit (maximum) for finer PWM resolution
    // Default is 10-bit (1024 ticks), max is 20-bit (1,048,576 ticks)
    // This should reduce quantization and stepping
    servo.setTimerWidth(20);
    
    servo.attach(servoPin); // Match TwoSkulls exactly
    setMinMaxDegrees(minDeg, maxDeg);
    setPosition(0); // Initialize to closed position

    // Log initialization details
    LOG_INFO(TAG, "Initializing servo on pin %d (min: %d, max: %d)", servoPin, minDegrees, maxDegrees);

    // Perform an initialization animation to demonstrate servo range
    LOG_DEBUG(TAG, "Servo animation init: %d (min) degrees", minDegrees);
    setPosition(minDegrees);
    LOG_DEBUG(TAG, "Servo animation init: %d (max) degrees", maxDegrees);
    delay(500);
    setPosition(maxDegrees);
    LOG_INFO(TAG, "Servo animation init complete; resetting to 0 degrees");
    delay(500);
    setPosition(minDegrees);
}

// Set the servo position within the allowed range
void ServoController::setPosition(int degrees)
{
    // Ensure the position is within the allowed range
    int constrainedDegrees = constrain(degrees, minDegrees, maxDegrees);

    // Write the position to the servo
    servo.write(constrainedDegrees);

    // Update the current position
    currentPosition = constrainedDegrees;
}

// Get the current servo position
int ServoController::getPosition() const
{
    return currentPosition;
}

// Set the minimum and maximum degrees for the servo
void ServoController::setMinMaxDegrees(int minDeg, int maxDeg)
{
    minDegrees = minDeg;
    maxDegrees = maxDeg;
}

// Map RMS (Root Mean Square) audio level to servo position
int ServoController::mapRMSToPosition(double rms, double silenceThreshold)
{
    // If RMS is below the silence threshold, return minimum position
    if (rms < silenceThreshold)
    {
        return minDegrees;
    }

    // Update maximum observed RMS if necessary
    if (rms > maxObservedRMS)
    {
        maxObservedRMS = rms;
    }

    // Normalize RMS value
    double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);

    // Apply non-linear mapping to create more natural movement
    double mappedValue = pow(normalizedRMS, 0.2); // 0.2 is the MOVE_EXPONENT

    // Define minimum jaw opening to prevent complete closure
    int minJawOpening = minDegrees + 5; // 5 degrees minimum opening

    // Map the value to servo position range
    return map(mappedValue * 1000, 0, 1000, minJawOpening, maxDegrees);
}

// Update servo position with smoothing and minimum movement threshold
void ServoController::updatePosition(int targetPosition, double alpha, int minMovementThreshold)
{
    // Load current smoothed position
    double current = smoothedPosition;

    // Apply exponential smoothing
    double updated = alpha * targetPosition + (1 - alpha) * current;

    // Store updated smoothed position
    smoothedPosition = updated;

    // Round and constrain the new position
    int newPosition = round(updated);
    newPosition = constrain(newPosition, minDegrees, maxDegrees);

    // Load last position
    int last = lastPosition;

    // Only update position if change exceeds minimum movement threshold
    if (abs(newPosition - last) > minMovementThreshold)
    {
        setPosition(newPosition);
        lastPosition = newPosition;
    }
}

// Add this new method implementation
void ServoController::smoothMove(int targetPosition, int duration) {
    int startPosition = currentPosition;
    unsigned long startTime = millis();
    unsigned long endTime = startTime + duration;
    
    shouldInterruptMovement = false;  // Reset the interrupt flag at the start of movement
    
    while (millis() < endTime) {
        if (shouldInterruptMovement) {
            shouldInterruptMovement = false;  // Reset the flag
            return;  // Exit the loop if interrupted
        }
        
        unsigned long currentTime = millis();
        float progress = static_cast<float>(currentTime - startTime) / duration;
        
        // Clamp progress to [0, 1]
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        
        // Linear interpolation
        int newPosition = startPosition + (targetPosition - startPosition) * progress;
        
        setPosition(newPosition);
        delay(20); // 20ms update rate to match servo control frequency
    }
    
    // Ensure we reach the final position if not interrupted
    setPosition(targetPosition);
}

void ServoController::interruptMovement() {
    shouldInterruptMovement = true;
}
