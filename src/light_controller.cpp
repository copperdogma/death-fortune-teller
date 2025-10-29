#include "light_controller.h"
#include <algorithm>
#include <math.h>

// LightController class constructor
// Initializes the pins for eye and mouth LEDs and sets initial brightness to off
LightController::LightController(int eyePin, int mouthPin)
    : _eyePin(eyePin),
      _mouthPin(mouthPin),
      _currentBrightness(BRIGHTNESS_OFF),
      _mouthMode(MouthMode::OFF),
      _mouthBright(PWM_MAX),
      _mouthPulseMin(40),
      _mouthPulseMax(PWM_MAX),
      _mouthPulsePeriodMs(1500),
      _mouthLastUpdateMs(0) {}

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

    configureMouthLED(PWM_MAX, 40, PWM_MAX, 1500);
    setMouthOff();
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

void LightController::configureMouthLED(uint8_t bright, uint8_t pulseMin, uint8_t pulseMax, unsigned long pulsePeriodMs)
{
    _mouthBright = constrain(bright, 0, PWM_MAX);
    _mouthPulseMin = constrain(pulseMin, 0, PWM_MAX);
    _mouthPulseMax = constrain(pulseMax, 0, PWM_MAX);
    if (_mouthPulseMin > _mouthPulseMax) {
        std::swap(_mouthPulseMin, _mouthPulseMax);
    }
    _mouthPulsePeriodMs = pulsePeriodMs < 200 ? 200 : pulsePeriodMs; // Prevent hyper-fast pulsing
}

void LightController::setMouthOff()
{
    _mouthMode = MouthMode::OFF;
    applyMouthBrightness(BRIGHTNESS_OFF);
}

void LightController::setMouthBright()
{
    _mouthMode = MouthMode::BRIGHT;
    applyMouthBrightness(_mouthBright);
}

void LightController::setMouthPulse()
{
    _mouthMode = MouthMode::PULSE;
    _mouthLastUpdateMs = 0;
}

void LightController::update()
{
    if (_mouthMode == MouthMode::PULSE) {
        updateMouthPulse(millis());
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
    MouthMode previousMode = _mouthMode;
    for (int i = 0; i < numBlinks; i++)
    {
        applyMouthBrightness(BRIGHTNESS_MAX); // Mouth LED on
        delay(100); // Mouth LED on for 100ms
        applyMouthBrightness(BRIGHTNESS_OFF); // Mouth LED off
        delay(100); // Mouth LED off for 100ms
    }
    if (previousMode == MouthMode::BRIGHT) {
        setMouthBright();
    } else if (previousMode == MouthMode::PULSE) {
        setMouthPulse();
    } else {
        setMouthOff();
    }
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

void LightController::applyMouthBrightness(uint8_t brightness)
{
    ledcWrite(PWM_CHANNEL_MOUTH, constrain(brightness, 0, PWM_MAX));
}

void LightController::updateMouthPulse(unsigned long now)
{
    if (_mouthPulsePeriodMs == 0) {
        applyMouthBrightness(_mouthPulseMax);
        return;
    }

    if (_mouthLastUpdateMs != 0 && now - _mouthLastUpdateMs < 15) {
        return; // Limit update rate to reduce jitter
    }
    _mouthLastUpdateMs = now;

    float phase = static_cast<float>(now % _mouthPulsePeriodMs) / static_cast<float>(_mouthPulsePeriodMs);
    float angle = phase * TWO_PI;
    float normalized = (sinf(angle) + 1.0f) * 0.5f; // Range 0..1
    uint8_t brightness = static_cast<uint8_t>(_mouthPulseMin + normalized * (_mouthPulseMax - _mouthPulseMin));
    applyMouthBrightness(brightness);
}
