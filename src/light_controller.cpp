#include "light_controller.h"

// LightController class constructor
// Initializes the pins for eye and mouth LEDs and sets initial brightness to off
LightController::LightController(int eyePin, int mouthPin)
    : _eyePin(eyePin), _mouthPin(mouthPin), _currentBrightness(BRIGHTNESS_OFF) {}

// Initializes the LightController
// Sets up PWM channels and attaches them to the eye and mouth pins
void LightController::begin()
{
    // Configure pins as outputs
    pinMode(_eyePin, OUTPUT);
    pinMode(_mouthPin, OUTPUT);

    // Set up PWM channels for eye and mouth LEDs
    // Using the same frequency and resolution for both channels
    ledcSetup(PWM_CHANNEL_EYE, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_MOUTH, PWM_FREQUENCY, PWM_RESOLUTION);

    // Attach the PWM channels to the respective pins
    ledcAttachPin(_eyePin, PWM_CHANNEL_EYE);
    ledcAttachPin(_mouthPin, PWM_CHANNEL_MOUTH);

    // Initialize eye LED to maximum brightness
    setEyeBrightness(BRIGHTNESS_MAX);
    
    // Initialize mouth LED to off (defaults to off unlike eye LED)
    ledcWrite(PWM_CHANNEL_MOUTH, BRIGHTNESS_OFF);
}

// Sets the brightness of the eye LED
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
            // Detach PWM from eye pin
            ledcDetachPin(_eyePin);

            // Set eye pin to HIGH
            pinMode(_eyePin, OUTPUT);
            digitalWrite(_eyePin, HIGH);
        }
        else
        {
            // If currently at MAX brightness (using digital HIGH), we need to reattach PWM
            if (_currentBrightness == BRIGHTNESS_MAX)
            {
                // Reattach PWM to eye pin
                ledcAttachPin(_eyePin, PWM_CHANNEL_EYE);
            }

            // Set PWM duty cycle
            ledcWrite(PWM_CHANNEL_EYE, brightness);
        }
        _currentBrightness = brightness;
    }
}

// Blinks the eye LED a specified number of times
// @param numBlinks: Number of times to blink
// @param onBrightness: Brightness level when eye is on
// @param offBrightness: Brightness level when eye is off
void LightController::blinkEyes(int numBlinks, int onBrightness, int offBrightness)
{
    for (int i = 0; i < numBlinks; i++)
    {
        setEyeBrightness(onBrightness);
        delay(100); // Eye on for 100ms
        setEyeBrightness(offBrightness);
        delay(100); // Eye off for 100ms
    }
    setEyeBrightness(onBrightness); // Ensure eye is on at the end of blinking sequence
}

// Blinks the mouth LED a specified number of times
// @param numBlinks: Number of times to blink
void LightController::blinkMouth(int numBlinks)
{
    for (int i = 0; i < numBlinks; i++)
    {
        ledcWrite(PWM_CHANNEL_MOUTH, BRIGHTNESS_MAX); // Mouth LED on
        delay(100); // Mouth LED on for 100ms
        ledcWrite(PWM_CHANNEL_MOUTH, BRIGHTNESS_OFF); // Mouth LED off
        delay(100); // Mouth LED off for 100ms
    }
    ledcWrite(PWM_CHANNEL_MOUTH, BRIGHTNESS_OFF); // Ensure mouth LED is off at the end (defaults to off)
}

// Blinks eye and mouth LEDs sequentially with non-blocking delay
// @param numBlinks: Number of times to blink each (eye first, then mouth after 1000ms delay)
void LightController::blinkLights(int numBlinks)
{
    // Start by blinking eye LED
    blinkEyes(numBlinks);
    
    // Wait 1000ms using non-blocking delay (using millis() to avoid blocking)
    unsigned long startTime = millis();
    while (millis() - startTime < 1000)
    {
        yield(); // Allow other tasks to run during wait
    }
    
    // Then blink mouth LED
    blinkMouth(numBlinks);
}