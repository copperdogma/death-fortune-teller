#include "light_controller.h"

// LightController class constructor
// Initializes the pins for left and right eyes and sets initial brightness to off
LightController::LightController(int leftEyePin, int rightEyePin)
    : _leftEyePin(leftEyePin), _rightEyePin(rightEyePin), _currentBrightness(BRIGHTNESS_OFF) {}

// Initializes the LightController
// Sets up PWM channels and attaches them to the eye pins
void LightController::begin()
{
    // Configure pins as outputs
    pinMode(_leftEyePin, OUTPUT);
    pinMode(_rightEyePin, OUTPUT);

    // Set up PWM channels for both eyes
    // Using the same frequency and resolution for both channels
    ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQUENCY, PWM_RESOLUTION);

    // Attach the PWM channels to the respective pins
    ledcAttachPin(_leftEyePin, PWM_CHANNEL_LEFT);
    ledcAttachPin(_rightEyePin, PWM_CHANNEL_RIGHT);

    // Initialize eyes to maximum brightness
    setEyeBrightness(BRIGHTNESS_MAX);
}

// Sets the brightness of both eyes
// @param brightness: uint8_t value between 0 (off) and 255 (max brightness)
// NOTE: I can't get PWM to go to full brightness, so we're setting the pin value to HIGH
//       when brightness is set to BRIGHTNESS_MAX, and using BRIGHTNESS_MAX as "dim".
void LightController::setEyeBrightness(uint8_t brightness)
{
    // Ensure brightness is within valid range (0-255)
    brightness = constrain(brightness, BRIGHTNESS_OFF, BRIGHTNESS_MAX);

    if (brightness != _currentBrightness)
    {
        if (brightness == BRIGHTNESS_MAX)
        {
            // Detach PWM from pins
            ledcDetachPin(_leftEyePin);
            ledcDetachPin(_rightEyePin);

            // Set pins to HIGH
            pinMode(_leftEyePin, OUTPUT);
            pinMode(_rightEyePin, OUTPUT);
            digitalWrite(_leftEyePin, HIGH);
            digitalWrite(_rightEyePin, HIGH);
        }
        else
        {
            if (_currentBrightness == BRIGHTNESS_DIM)
            {
                _currentBrightness = BRIGHTNESS_MAX;
            }
            // Reattach PWM to pins
            ledcAttachPin(_leftEyePin, PWM_CHANNEL_LEFT);
            ledcAttachPin(_rightEyePin, PWM_CHANNEL_RIGHT);

            // Set PWM duty cycle
            ledcWrite(PWM_CHANNEL_LEFT, _currentBrightness);
            ledcWrite(PWM_CHANNEL_RIGHT, _currentBrightness);
        }
        _currentBrightness = brightness;
    }
}

// Blinks the eyes a specified number of times
// @param numBlinks: Number of times to blink
// @param onBrightness: Brightness level when eyes are on
// @param offBrightness: Brightness level when eyes are off
void LightController::blinkEyes(int numBlinks, int onBrightness, int offBrightness)
{
    for (int i = 0; i < numBlinks; i++)
    {
        setEyeBrightness(onBrightness);
        delay(100); // Eyes on for 100ms
        setEyeBrightness(offBrightness);
        delay(100); // Eyes off for 100ms
    }
    setEyeBrightness(onBrightness); // Ensure eyes are on at the end of blinking sequence
}