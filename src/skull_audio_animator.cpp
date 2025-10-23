/*
    This file handles the animation of the skulls based on the audio.
    It uses the audio player to get the audio data and the servo controller to move the jaw.
    It also uses the light controller to control the eyes.

    Although it provides pass-throughs for playing audio, it has no effect on the playing state.
    It only reacts to what is being currently played, which is entirely controlled by the audio player.

    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

      Frame(int ch1, int ch2){
        channel1 = ch1;
        channel2 = ch2;
      }
*/

#include "skull_audio_animator.h"
#include "logging_manager.h"

static constexpr const char* TAG = "SkullAnimator";
#include <cmath>
#include <algorithm>
#include <Arduino.h>

// Constructor for SkullAudioAnimator class
// Initializes the animator with necessary controllers and parameters
SkullAudioAnimator::SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController,
                                       std::vector<ParsedSkit> &skits, SDCardManager &sdCardManager, int servoMinDegrees, int servoMaxDegrees)
    : m_servoController(servoController),
      m_lightController(lightController),
      m_sdCardManager(sdCardManager),
      m_isPrimary(isPrimary),
      m_skits(skits),
      m_servoMinDegrees(servoMinDegrees),
      m_servoMaxDegrees(servoMaxDegrees),
      m_isCurrentlySpeaking(false),
      m_currentSkitLineNumber(-1),
      m_smoothedAmplitude(0.0),
      m_previousJawPosition(servoMinDegrees),
      FFT(vReal, vImag, SAMPLES, SAMPLE_RATE)
{
}

// Main function to process incoming audio frames and update animations
void SkullAudioAnimator::processAudioFrames(const Frame *frames, int32_t frameCount, const String &currentFile, unsigned long playbackTime)
{
    // Update internal state based on new audio data
    m_currentFile = currentFile;
    m_currentPlaybackTime = playbackTime;
    m_isAudioPlaying = (frameCount > 0);

    // Serial.printf("SkullAudioAnimator::processAudioFrames() m_currentFile: %s, m_isAudioPlaying: %s, frameCount: %d, isSpeaking: %s\n", m_currentFile.c_str(), m_isAudioPlaying ? "true" : "false", frameCount, m_isCurrentlySpeaking ? "true" : "false");

    // Process audio frames for various animations
    updateSkit();
    updateEyes();
    updateJawPosition(frames, frameCount);
}

void SkullAudioAnimator::setPlaybackEnded(const String &filePath)
{
    // TODO: is this much tracking necessary??
    m_currentFile = "";
    m_currentPlaybackTime = 0;
    m_isAudioPlaying = false;
    m_currentAudioFilePath = "";
    m_currentSkit = ParsedSkit();
    m_currentSkitLineNumber = -1;

    LOG_DEBUG(TAG, "Playback ended: %s", filePath.c_str());

    // Process audio frames for various animations
    updateSkit();
    updateEyes();
}

// Updates the current skit state and speaking status based on audio playback
// States:
// - No audio files = not speaking
// - Non-skit audio file = speaking
// - Skit, speaking line = speaking
// - Skit, not speaking line = not speaking
void SkullAudioAnimator::updateSkit()
{
    // If no audio is playing, set speaking state to false
    if (!m_isAudioPlaying)
    {
        setSpeakingState(false);
        return;
    }

    // Handle case when current file is emptyPlaying non-skit audio file
    if (m_currentFile.isEmpty())
    {
        setSpeakingState(false);
        return;
    }

    // If we're playing a new file, parse the skit and reset the line number
    if (m_currentFile != m_currentAudioFilePath)
    {
        m_currentAudioFilePath = m_currentFile;
        m_currentSkitLineNumber = -1;

        m_currentSkit = findSkitByName(m_skits, m_currentFile);
        if (m_currentSkit.lines.empty())
        {
            LOG_DEBUG(TAG, "Non-skit audio file playing (file=%s, time=%lu, currentPath=%s)",
                     m_currentFile.c_str(), m_currentPlaybackTime, m_currentAudioFilePath.c_str());
            setSpeakingState(true);
            return;
        }

        LOG_INFO(TAG, "Playing new skit at %lu ms: %s", m_currentPlaybackTime, m_currentSkit.audioFile.c_str());

        // Filter skit lines for the current skull (primary or secondary)
        std::vector<ParsedSkitLine> lines;
        for (const auto &line : m_currentSkit.lines)
        {
            if ((line.speaker == 'A' && m_isPrimary) || (line.speaker == 'B' && !m_isPrimary))
            {
                lines.push_back(line);
            }
        }
        LOG_INFO(TAG, "Parsed skit '%s' with %d lines (%d applicable)",
                 m_currentSkit.audioFile.c_str(), m_currentSkit.lines.size(), lines.size());
        m_currentSkit.lines = lines;
    }

    // If we're playing a non-skit, we're speaking
    if (m_currentSkit.lines.empty())
    {
        setSpeakingState(true);
        return;
    }

    // Find the current speaking line in the skit based on playback time
    size_t originalLineNumber = m_currentSkitLineNumber;
    bool foundLine = false;
    for (const auto &line : m_currentSkit.lines)
    {
        // We need to clip the end of the line to avoid overlap with the next line.
        // Even if you get the timings exactly right in the skit file, I believe this is taking the buffer into account.
        if (m_currentPlaybackTime >= line.timestamp && m_currentPlaybackTime < line.timestamp + line.duration - SKIT_AUDIO_LINE_OFFSET)
        {
            m_currentSkitLineNumber = line.lineNumber;
            foundLine = true;
            break;
        }
    }

    // Log when a new line starts speaking
    if (m_currentSkitLineNumber != originalLineNumber)
    {
        LOG_DEBUG(TAG, "Now speaking line %d at %lu ms", m_currentSkitLineNumber, m_currentPlaybackTime);
    }

    setSpeakingState(foundLine);

    // Log when a line finishes speaking
    if (m_isCurrentlySpeaking && !foundLine)
    {
        LOG_DEBUG(TAG, "Ended speaking line %d at %lu ms", m_currentSkitLineNumber, m_currentPlaybackTime);
    }
}

// Updates the eye brightness based on the speaking state
void SkullAudioAnimator::updateEyes()
{
    if (m_isCurrentlySpeaking)
    {
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
    }
    else
    {
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
    }
}

void SkullAudioAnimator::updateJawPosition(const Frame *frames, int32_t frameCount)
{
    // Interrupt any ongoing smooth movement
    m_servoController.interruptMovement();

    if (frameCount > 0)
    {
        // Compute RMS amplitude over the frames
        double rmsAmplitude = calculateRMSFromFrames(frames, frameCount);

        // Apply exponential smoothing to the amplitude
        m_smoothedAmplitude = AMPLITUDE_SMOOTHING_FACTOR * rmsAmplitude + (1 - AMPLITUDE_SMOOTHING_FACTOR) * m_smoothedAmplitude;

        // Apply gain and adjust amplitude
        double adjustedAmplitude = m_smoothedAmplitude * AMPLITUDE_GAIN;
        adjustedAmplitude = std::min(adjustedAmplitude, MAX_EXPECTED_AMPLITUDE);

        // Implement a threshold to avoid small movements
        if (adjustedAmplitude < AMPLITUDE_THRESHOLD)
        {
            adjustedAmplitude = 0.0;
        }

        // Map the adjusted amplitude to jaw position
        int targetJawPosition = mapFloat(adjustedAmplitude, 0.0, MAX_EXPECTED_AMPLITUDE, m_servoMinDegrees, m_servoMaxDegrees);

        // Smooth the jaw position to reduce jitter
        int jawPosition = static_cast<int>(JAW_POSITION_SMOOTHING_FACTOR * targetJawPosition + (1 - JAW_POSITION_SMOOTHING_FACTOR) * m_previousJawPosition);

        // Update the servo position
        m_servoController.setPosition(jawPosition);

        // Store the previous jaw position for the next iteration
        m_previousJawPosition = jawPosition;

        // For debugging purposes
        // Serial.printf("RMS Amplitude: %.2f, Adjusted Amplitude: %.2f, Jaw Position: %d\n", rmsAmplitude, adjustedAmplitude, jawPosition);
    }
    else
    {
        // Close the jaw when there's no audio
        m_servoController.setPosition(m_servoMinDegrees);
        m_previousJawPosition = m_servoMinDegrees;
        m_smoothedAmplitude = 0.0;
    }
}

double SkullAudioAnimator::calculateRMSFromFrames(const Frame *frames, int32_t frameCount)
{
    double sum = 0.0;
    for (int32_t i = 0; i < frameCount; i++)
    {
        int32_t sample1 = frames[i].channel1;
        int32_t sample2 = frames[i].channel2;
        sum += sample1 * sample1;
        sum += sample2 * sample2;
    }
    int numSamples = frameCount * 2; // Two channels
    return sqrt(sum / numSamples);
}

int SkullAudioAnimator::mapFloat(double x, double in_min, double in_max, int out_min, int out_max)
{
    return static_cast<int>((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

// Finds a skit by its name in the list of parsed skits
ParsedSkit SkullAudioAnimator::findSkitByName(const std::vector<ParsedSkit> &skits, const String &name)
{
    for (const auto &skit : skits)
    {
        if (skit.audioFile == name)
        {
            return skit;
        }
    }
    return ParsedSkit();
}

// Sets the callback function for speaking state changes
void SkullAudioAnimator::setSpeakingStateCallback(SpeakingStateCallback callback)
{
    m_speakingStateCallback = std::move(callback);
}

// Updates the speaking state and triggers the callback if changed
void SkullAudioAnimator::setSpeakingState(bool isSpeaking)
{
    if (m_isCurrentlySpeaking != isSpeaking)
    {
        m_isCurrentlySpeaking = isSpeaking;
        if (m_speakingStateCallback)
        {
            m_speakingStateCallback(m_isCurrentlySpeaking);
        }
    }
}
