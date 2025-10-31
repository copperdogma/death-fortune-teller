#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

#include "servo_controller.h"
#include "arduinoFFT.h"
#include "light_controller.h"
#include "parsed_skit.h"
#include "BluetoothA2DPSource.h" // For Frame definition
#include <vector>
#include <Arduino.h>
#include <functional>

// TODO: Should probably be defined by the audioPlayer and passed in from it, either during init or via processAudioFrames()
#define SAMPLES 256
#define SAMPLE_RATE 44100

// Forward declarations
class SDCardManager;

// SkullAudioAnimator class handles the animation of skulls based on audio input
class SkullAudioAnimator
{
public:
    // Constructor: initializes the animator with necessary controllers and parameters
    // isPrimary: true for primary/coordinator animatronic, false for secondary/tertiary/etc.
    SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController,
                       std::vector<ParsedSkit> &skits, SDCardManager &sdCardManager, int servoMinDegrees, int servoMaxDegrees);

    // Finds a skit by its name in the list of parsed skits
    ParsedSkit findSkitByName(const std::vector<ParsedSkit> &skits, const String &name);

    // Returns the current speaking state of the skull
    bool isCurrentlySpeaking() { return m_isCurrentlySpeaking; }

    // Main function to process incoming audio frames and update animations
    void processAudioFrames(const Frame *frames, int32_t frameCount, const String &currentFile, unsigned long playbackTime);

    // Typedef for the speaking state callback function
    using SpeakingStateCallback = std::function<void(bool)>;

    // Sets the callback function for speaking state changes
    void setSpeakingStateCallback(SpeakingStateCallback callback);

    // Sets the playback ended state
    void setPlaybackEnded(const String &filePath);

    // Holds the jaw at a fixed position when no audio is playing (e.g., wait-for-finger prompt)
    void setJawHoldOverride(bool active, int holdPositionDegrees = 0);

private:
    ServoController &m_servoController;
    LightController &m_lightController;
    SDCardManager &m_sdCardManager;
    bool m_isPrimary;
    std::vector<ParsedSkit> &m_skits;
    String m_currentAudioFilePath;
    bool m_isCurrentlySpeaking;
    size_t m_currentSkitLineNumber;
    ParsedSkit m_currentSkit;
    double vReal[SAMPLES];
    double vImag[SAMPLES];
    arduinoFFT FFT;
    double m_smoothedAmplitude; // Exponential smoothing of amplitude
    int m_previousJawPosition;  // Previous jaw position for smoothing

    String m_currentFile;
    unsigned long m_currentPlaybackTime;
    bool m_isAudioPlaying;

    // Constants for jaw position calculation

    // Controls how much weight is given to new amplitude vs. previous smoothed value.
    // Lower value (e.g., 0.1) results in more smoothing, reducing sensitivity to transient peaks.
    static constexpr double AMPLITUDE_SMOOTHING_FACTOR = 0.1;

    // Controls the smoothing of jaw movements.
    // Helps create fluid transitions between positions, reducing jitter.
    static constexpr double JAW_POSITION_SMOOTHING_FACTOR = 0.2;

    // Amplifies the smoothed amplitude to utilize the full range of the servo.
    // Adjust based on testing to achieve desired jaw movement range.
    static constexpr double AMPLITUDE_GAIN = 5.0;

    // Sets the upper limit for mapping amplitudes to servo positions.
    // Adjust based on your audio levels to prevent over-extension of the jaw.
    static constexpr double MAX_EXPECTED_AMPLITUDE = 15000.0;

    // Determines the minimum amplitude required to start moving the jaw.
    // Helps achieve "mostly open" and "mostly closed" effect by ignoring minor fluctuations.
    static constexpr double AMPLITUDE_THRESHOLD = 1000.0;

    // Updates the jaw position based on the audio amplitude
    void updateJawPosition(const Frame *frames, int32_t frameCount);

    // Updates the eye brightness based on the speaking state
    void updateEyes();

    // Updates the current skit state and speaking status based on audio playback
    void updateSkit();

    // Calculates the Root Mean Square (RMS) of the audio samples
    double calculateRMSFromFrames(const Frame *frames, int32_t frameCount);
    int mapFloat(double x, double in_min, double in_max, int out_min, int out_max);

    int m_servoMinDegrees;
    int m_servoMaxDegrees;
    bool m_jawHoldActive;
    int m_jawHoldPosition;

    SpeakingStateCallback m_speakingStateCallback;

    // Updates the speaking state and triggers the callback if changed
    void setSpeakingState(bool isSpeaking);

    static const unsigned long SKIT_AUDIO_LINE_OFFSET = 0; // Milliseconds to clip off the end of a skit audio line to avoid overlap

    static constexpr int32_t MAX_AUDIO_AMPLITUDE = 500; // Maximum value for an int16_t = 32767
};

#endif // SKULL_AUDIO_ANIMATOR_H
