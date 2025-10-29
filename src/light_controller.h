#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>

// PWM configuration constants
#define PWM_FREQUENCY 5000  // PWM frequency in Hz
#define PWM_RESOLUTION 8    // PWM resolution in bits
#define PWM_MAX 255         // Maximum PWM value (2^8 - 1)
#define PWM_CHANNEL_EYE 6   // PWM channel for eye LED (moved high to avoid servo collisions)
#define PWM_CHANNEL_MOUTH 7 // PWM channel for mouth LED (moved high to avoid servo collisions)

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
    bool isEyePatternActive() const;
    void startEyeBlinkPattern(int numBlinks,
                              unsigned long onDurationMs = 100,
                              unsigned long offDurationMs = 100,
                              unsigned long repeatDelayMs = 700,
                              uint8_t onBrightness = BRIGHTNESS_MAX,
                              uint8_t offBrightness = BRIGHTNESS_OFF,
                              int repeatSets = -1,
                              const char *label = nullptr);
    void stopEyeBlinkPattern();

    // Configure mouth LED behavior (brightness + pulse parameters)
    void configureMouthLED(uint8_t bright, uint8_t pulseMin, uint8_t pulseMax, unsigned long pulsePeriodMs);

    // Mouth LED control helpers
    void setMouthOff();
    void setMouthBright();
    void setMouthPulse();
    void startMouthBlinkSequence(int numBlinks,
                                 unsigned long onDurationMs = 120,
                                 unsigned long offDurationMs = 120,
                                 uint8_t blinkBrightness = PWM_MAX,
                                 bool restorePreviousMode = false,
                                 const char *label = nullptr);
    bool isMouthBlinking() const;

    // Update function to be called from loop() for animations
    void update();

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

    enum class MouthMode {
        OFF,
        BRIGHT,
        PULSE,
        BLINKING
    };

    MouthMode _mouthMode;
    uint8_t _mouthBright;
    uint8_t _mouthPulseMin;
    uint8_t _mouthPulseMax;
    unsigned long _mouthPulsePeriodMs;
    unsigned long _mouthLastUpdateMs;
    MouthMode _mouthPreviousMode;
    bool _mouthBlinkRestorePrevious;

    struct MouthBlinkState {
        bool active;
        int blinksRemaining;
        unsigned long onDurationMs;
        unsigned long offDurationMs;
        unsigned long nextToggleMs;
        bool isOnPhase;
        uint8_t onBrightness;
        uint8_t offBrightness;
    };

    struct EyePatternState {
        bool active;
        bool indefinite;
        int blinksPerSet;
        int completedBlinks;
        unsigned long onDurationMs;
        unsigned long offDurationMs;
        unsigned long repeatDelayMs;
        unsigned long nextToggleMs;
        bool isOnPhase;
        uint8_t onBrightness;
        uint8_t offBrightness;
        uint8_t storedNormalBrightness;
        int setsRemaining;
    };

    MouthBlinkState _mouthBlink;
    EyePatternState _eyePattern;

    void applyMouthBrightness(uint8_t brightness);
    void updateMouthPulse(unsigned long now);
    void updateMouthBlink(unsigned long now);
    void updateEyePattern(unsigned long now);
    void applyEyeBrightness(uint8_t brightness);
};

#endif // LIGHT_CONTROLLER_H
