#include "light_controller.h"
#include <algorithm>
#include <math.h>
#include "logging_manager.h"
#include <cstdio>

static constexpr const char *LIGHT_TAG = "LightController";

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
      _mouthLastUpdateMs(0),
      _mouthPreviousMode(MouthMode::OFF),
      _mouthBlinkRestorePrevious(false),
      _mouthBlink{false, 0, 120, 120, 0, false, PWM_MAX, BRIGHTNESS_OFF},
      _eyePattern{false, false, 0, 0, 100, 100, 700, 0, false, BRIGHTNESS_MAX, BRIGHTNESS_OFF, BRIGHTNESS_MAX, 0} {}

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

// Sets the brightness of the eye LED unless an error pattern is active
void LightController::setEyeBrightness(uint8_t brightness)
{
    brightness = constrain(brightness, BRIGHTNESS_OFF, BRIGHTNESS_MAX);
    if (_eyePattern.active) {
        _eyePattern.storedNormalBrightness = brightness;
        return;
    }
    applyEyeBrightness(brightness);
    _eyePattern.storedNormalBrightness = _currentBrightness;
}

bool LightController::isEyePatternActive() const
{
    return _eyePattern.active;
}

void LightController::startEyeBlinkPattern(int numBlinks,
                                           unsigned long onDurationMs,
                                           unsigned long offDurationMs,
                                           unsigned long repeatDelayMs,
                                           uint8_t onBrightness,
                                           uint8_t offBrightness,
                                           int repeatSets,
                                           const char *label)
{
    numBlinks = std::max(1, numBlinks);
    _eyePattern.active = true;
    _eyePattern.indefinite = (repeatSets < 0);
    _eyePattern.setsRemaining = _eyePattern.indefinite ? 0 : std::max(1, repeatSets);
    _eyePattern.blinksPerSet = numBlinks;
    _eyePattern.completedBlinks = 0;
    _eyePattern.onDurationMs = std::max<unsigned long>(10, onDurationMs);
    _eyePattern.offDurationMs = std::max<unsigned long>(10, offDurationMs);
    _eyePattern.repeatDelayMs = repeatDelayMs;
    _eyePattern.nextToggleMs = 0;
    _eyePattern.isOnPhase = false;
    _eyePattern.onBrightness = constrain(onBrightness, BRIGHTNESS_OFF, BRIGHTNESS_MAX);
    _eyePattern.offBrightness = constrain(offBrightness, BRIGHTNESS_OFF, BRIGHTNESS_MAX);
    _eyePattern.storedNormalBrightness = _currentBrightness;
    const char *labelText = (label && label[0]) ? label : "unspecified";
    char repeatBuffer[16];
    const char *repeatDescription;
    if (_eyePattern.indefinite) {
        repeatDescription = "infinite";
    } else {
        snprintf(repeatBuffer, sizeof(repeatBuffer), "%d", _eyePattern.setsRemaining);
        repeatDescription = repeatBuffer;
    }
    LOG_INFO(LIGHT_TAG,
             "Eye blink pattern start (%s): blinks=%d on=%lums off=%lums repeats=%s delay=%lums bright=%u/%u",
             labelText,
             numBlinks,
             _eyePattern.onDurationMs,
             _eyePattern.offDurationMs,
             repeatDescription,
             _eyePattern.repeatDelayMs,
             _eyePattern.onBrightness,
             _eyePattern.offBrightness);
}

void LightController::stopEyeBlinkPattern()
{
    if (!_eyePattern.active) {
        return;
    }
    _eyePattern.active = false;
    applyEyeBrightness(_eyePattern.storedNormalBrightness);
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
    _mouthBlink.active = false;
    _mouthMode = MouthMode::OFF;
    applyMouthBrightness(BRIGHTNESS_OFF);
}

void LightController::setMouthBright()
{
    _mouthBlink.active = false;
    _mouthMode = MouthMode::BRIGHT;
    applyMouthBrightness(_mouthBright);
}

void LightController::setMouthPulse()
{
    _mouthBlink.active = false;
    _mouthMode = MouthMode::PULSE;
    _mouthLastUpdateMs = 0;
}

void LightController::startMouthBlinkSequence(int numBlinks,
                                              unsigned long onDurationMs,
                                              unsigned long offDurationMs,
                                              uint8_t blinkBrightness,
                                              bool restorePreviousMode,
                                              const char *label)
{
    numBlinks = std::max(1, numBlinks);
    _mouthPreviousMode = _mouthMode == MouthMode::BLINKING ? MouthMode::OFF : _mouthMode;
    _mouthBlinkRestorePrevious = restorePreviousMode;
    _mouthMode = MouthMode::BLINKING;
    _mouthBlink.active = true;
    _mouthBlink.blinksRemaining = numBlinks;
    _mouthBlink.onDurationMs = std::max<unsigned long>(10, onDurationMs);
    _mouthBlink.offDurationMs = std::max<unsigned long>(10, offDurationMs);
    _mouthBlink.nextToggleMs = 0;
    _mouthBlink.isOnPhase = false;
    _mouthBlink.onBrightness = constrain(blinkBrightness, 0, PWM_MAX);
    _mouthBlink.offBrightness = BRIGHTNESS_OFF;
    LOG_INFO(LIGHT_TAG,
             "Mouth blink pattern start (%s): blinks=%d on=%lums off=%lums brightness=%u restore=%s",
             label && label[0] ? label : "unspecified",
             numBlinks,
             _mouthBlink.onDurationMs,
             _mouthBlink.offDurationMs,
             _mouthBlink.onBrightness,
             restorePreviousMode ? "true" : "false");
}

bool LightController::isMouthBlinking() const
{
    return _mouthBlink.active;
}

void LightController::update()
{
    unsigned long now = millis();
    if (_mouthMode == MouthMode::PULSE) {
        updateMouthPulse(now);
    } else if (_mouthMode == MouthMode::BLINKING) {
        updateMouthBlink(now);
    }

    if (_eyePattern.active) {
        updateEyePattern(now);
    }
}
// Blinks the eye LED a specified number of times
// @param numBlinks: Number of times to blink
// @param onBrightness: Brightness level when eye is on
// @param offBrightness: Brightness level when eye is off
void LightController::blinkEyes(int numBlinks, int onBrightness, int offBrightness)
{
    LOG_INFO(LIGHT_TAG,
             "Blocking eye blink pattern: blinks=%d bright=%d/%d",
             numBlinks,
             onBrightness,
             offBrightness);
    for (int i = 0; i < numBlinks; i++)
    {
        setEyeBrightness(onBrightness);
        delay(200); // Eye on for 100ms
        setEyeBrightness(offBrightness);
        delay(200); // Eye off for 100ms
    }
    setEyeBrightness(onBrightness); // Ensure eye is on at the end of blinking sequence
}

// Blinks the mouth LED a specified number of times
// @param numBlinks: Number of times to blink
void LightController::blinkMouth(int numBlinks)
{
    LOG_INFO(LIGHT_TAG,
             "Blocking mouth blink pattern: blinks=%d",
             numBlinks);
    MouthMode previousMode = _mouthMode;
    for (int i = 0; i < numBlinks; i++)
    {
        applyMouthBrightness(BRIGHTNESS_MAX); // Mouth LED on
        delay(200); // Mouth LED on for 100ms
        applyMouthBrightness(BRIGHTNESS_OFF); // Mouth LED off
        delay(200); // Mouth LED off for 100ms
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
    LOG_INFO(LIGHT_TAG,
             "Blocking combo blink pattern: blinks=%d",
             numBlinks);
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

void LightController::updateMouthBlink(unsigned long now)
{
    if (!_mouthBlink.active) {
        _mouthMode = _mouthBlinkRestorePrevious ? _mouthPreviousMode : MouthMode::OFF;
        if (_mouthMode == MouthMode::OFF) {
            applyMouthBrightness(BRIGHTNESS_OFF);
        } else if (_mouthMode == MouthMode::BRIGHT) {
            applyMouthBrightness(_mouthBright);
        } else if (_mouthMode == MouthMode::PULSE) {
            _mouthLastUpdateMs = 0;
        }
        return;
    }

    if (_mouthBlink.nextToggleMs != 0 && now < _mouthBlink.nextToggleMs) {
        return;
    }

    if (!_mouthBlink.isOnPhase) {
        applyMouthBrightness(_mouthBlink.onBrightness);
        _mouthBlink.isOnPhase = true;
        _mouthBlink.nextToggleMs = now + _mouthBlink.onDurationMs;
    } else {
        applyMouthBrightness(_mouthBlink.offBrightness);
        _mouthBlink.isOnPhase = false;
        _mouthBlink.nextToggleMs = now + _mouthBlink.offDurationMs;
        _mouthBlink.blinksRemaining--;
        if (_mouthBlink.blinksRemaining <= 0) {
            _mouthBlink.active = false;
            _mouthMode = _mouthBlinkRestorePrevious ? _mouthPreviousMode : MouthMode::OFF;
            if (_mouthMode == MouthMode::OFF) {
                applyMouthBrightness(BRIGHTNESS_OFF);
            } else if (_mouthMode == MouthMode::BRIGHT) {
                applyMouthBrightness(_mouthBright);
            } else if (_mouthMode == MouthMode::PULSE) {
                _mouthLastUpdateMs = 0;
            }
        }
    }
}

void LightController::applyEyeBrightness(uint8_t brightness)
{
    brightness = constrain(brightness, BRIGHTNESS_OFF, BRIGHTNESS_MAX);

    if (brightness != _currentBrightness)
    {
        if (brightness == BRIGHTNESS_MAX)
        {
            ledcDetachPin(_eyePin);
            pinMode(_eyePin, OUTPUT);
            digitalWrite(_eyePin, HIGH);
        }
        else
        {
            if (_currentBrightness == BRIGHTNESS_MAX)
            {
                ledcAttachPin(_eyePin, PWM_CHANNEL_EYE);
            }
            ledcWrite(PWM_CHANNEL_EYE, brightness);
        }
        _currentBrightness = brightness;
    }
}

void LightController::updateEyePattern(unsigned long now)
{
    if (!_eyePattern.active) {
        return;
    }

    if (_eyePattern.nextToggleMs != 0 && now < _eyePattern.nextToggleMs) {
        return;
    }

    if (!_eyePattern.isOnPhase) {
        applyEyeBrightness(_eyePattern.onBrightness);
        _eyePattern.isOnPhase = true;
        _eyePattern.nextToggleMs = now + _eyePattern.onDurationMs;
    } else {
        applyEyeBrightness(_eyePattern.offBrightness);
        _eyePattern.isOnPhase = false;
        _eyePattern.completedBlinks++;

        if (_eyePattern.completedBlinks >= _eyePattern.blinksPerSet) {
            _eyePattern.completedBlinks = 0;
            if (_eyePattern.indefinite) {
                _eyePattern.nextToggleMs = now + _eyePattern.repeatDelayMs;
            } else {
                _eyePattern.setsRemaining--;
                if (_eyePattern.setsRemaining > 0) {
                    _eyePattern.nextToggleMs = now + _eyePattern.repeatDelayMs;
                } else {
                    _eyePattern.active = false;
                    applyEyeBrightness(_eyePattern.storedNormalBrightness);
                    return;
                }
            }
        } else {
            _eyePattern.nextToggleMs = now + _eyePattern.offDurationMs;
        }
    }
}
