#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>

// PWM configuration constants
#define PWM_FREQUENCY 5000  // PWM frequency in Hz
#define PWM_RESOLUTION 8    // PWM resolution in bits
#define PWM_MAX 255         // Maximum PWM value (2^8 - 1)
#define PWM_CHANNEL_LEFT 0  // PWM channel for left eye
#define PWM_CHANNEL_RIGHT 1 // PWM channel for right eye

class LightController
{
public:
    // Brightness constants
    // NOTE: I can't get PWM to go to full brightness, so these constants don't really do much.
    //       See LightController::setEyeBrightness() for actual implementation.
    static const uint8_t BRIGHTNESS_MAX = PWM_MAX; // Maximum brightness level
    static const uint8_t BRIGHTNESS_DIM = 100;     // Dimmed brightness level
    static const uint8_t BRIGHTNESS_OFF = 0;       // Lights off

    // Constructor: Initializes the pins for left and right eyes
    LightController(int leftEyePin, int rightEyePin);

    // Initializes the LightController: Sets up PWM channels and attaches them to the eye pins
    void begin();

    // Sets the brightness of both eyes
    // @param brightness: uint8_t value between 0 (off) and 255 (max brightness)
    void setEyeBrightness(uint8_t brightness);

    // Blinks the eyes a specified number of times
    // @param numBlinks: Number of times to blink
    // @param onBrightness: Brightness level when eyes are on (default: BRIGHTNESS_MAX)
    // @param offBrightness: Brightness level when eyes are off (default: BRIGHTNESS_OFF)
    void blinkEyes(int numBlinks, int onBrightness = BRIGHTNESS_MAX, int offBrightness = BRIGHTNESS_OFF);

private:
    int _leftEyePin;        // Pin number for the left eye LED
    int _rightEyePin;       // Pin number for the right eye LED
    int _currentBrightness; // Current brightness level of both eyes
};

#endif // LIGHT_CONTROLLER_H