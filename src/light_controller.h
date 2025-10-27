#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>

// PWM configuration constants
#define PWM_FREQUENCY 5000  // PWM frequency in Hz
#define PWM_RESOLUTION 8    // PWM resolution in bits
#define PWM_MAX 255         // Maximum PWM value (2^8 - 1)
#define PWM_CHANNEL_EYE 0   // PWM channel for eye LED
#define PWM_CHANNEL_MOUTH 1 // PWM channel for mouth LED

class LightController
{
public:
    // Brightness constants
    // NOTE: I can't get PWM to go to full brightness, so these constants don't really do much.
    //       See LightController::setEyeBrightness() for actual implementation.
    static const uint8_t BRIGHTNESS_MAX = PWM_MAX; // Maximum brightness level
    static const uint8_t BRIGHTNESS_DIM = 100;     // Dimmed brightness level
    static const uint8_t BRIGHTNESS_OFF = 0;       // Lights off

    // Constructor: Initializes the pins for eye and mouth LEDs
    LightController(int eyePin, int mouthPin);

    // Initializes the LightController: Sets up PWM channels and attaches them to the eye and mouth pins
    void begin();

    // Sets the brightness of the eye LED
    // @param brightness: uint8_t value between 0 (off) and 255 (max brightness)
    void setEyeBrightness(uint8_t brightness);

    // Blinks the eye LED a specified number of times
    // @param numBlinks: Number of times to blink
    // @param onBrightness: Brightness level when eye is on (default: BRIGHTNESS_MAX)
    // @param offBrightness: Brightness level when eye is off (default: BRIGHTNESS_OFF)
    void blinkEyes(int numBlinks, int onBrightness = BRIGHTNESS_MAX, int offBrightness = BRIGHTNESS_OFF);

    // Blinks the mouth LED a specified number of times
    // @param numBlinks: Number of times to blink
    void blinkMouth(int numBlinks);

    // Blinks eye and mouth LEDs sequentially
    // @param numBlinks: Number of times to blink each (eye first, then mouth after 1000ms delay)
    void blinkLights(int numBlinks);

private:
    int _eyePin;            // Pin number for the eye LED (GPIO 32)
    int _mouthPin;          // Pin number for the mouth LED (GPIO 33)
    int _currentBrightness; // Current brightness level of the eye LED
};

#endif // LIGHT_CONTROLLER_H
