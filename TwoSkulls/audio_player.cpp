/*
    This is the audio player for the skull.

    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

    struct __attribute__((packed)) Frame {
        int16_t channel1;
        int16_t channel2;

        Frame(int v=0){
            channel1 = channel2 = v;
        }

        Frame(int ch1, int ch2){
            channel1 = ch1;
            channel2 = ch2;
        }
    };
*/

#include "audio_player.h"
#include "sd_card_manager.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <mutex>
#include <thread>

// Define a constant for an undefined buffer position
constexpr size_t AudioPlayer::BUFFER_POS_UNDEFINED;

// Keep track of the last printed second for logging purposes
static unsigned long lastPrintedSecond = 0;

AudioPlayer::AudioPlayer(SDCardManager &sdCardManager)
    : m_writePos(0), m_readPos(0), m_bufferFilled(0), m_totalBufferWritePos(0), m_totalBufferReadPos(0),
      m_currentBufferingFilePath(""),
      m_fileStartBufferPos(BUFFER_POS_UNDEFINED), m_fileStartPath(""), m_fileEndBufferPos(BUFFER_POS_UNDEFINED), m_fileEndPath(""),
      m_isAudioPlaying(false), m_muted(false), m_playbackStartTime(0), m_currentPlayingFilePath(""),
      m_sdCardManager(sdCardManager), m_bytesPlayed(0)
{
}

// Add a new audio file to the playback queue
void AudioPlayer::playNext(String filePath)
{
    if (filePath.length() > 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Ensure thread-safe access to shared resources
        audioQueue.push(filePath.c_str());
        Serial.printf("AudioPlayer::playNext() Added file to queue: %s ...\n", filePath.c_str());
    }
}

// Provide audio frames to the audio output stream
int32_t AudioPlayer::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Exit if there's no data available to read
    if (m_bufferFilled == 0)
    {
        m_currentPlayingFilePath = "";
        m_isAudioPlaying = false;
        fillBuffer(); // Try to fill the buffer
        return 0;
    }

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = 0;

    // Read data from buffer, handling wrap-around if necessary
    while (bytesRead < bytesToRead && m_bufferFilled > 0)
    {
        size_t chunkSize = std::min(bytesToRead - bytesRead, m_bufferFilled);
        size_t firstChunkSize = std::min(chunkSize, AUDIO_BUFFER_SIZE - m_readPos);
        memcpy((uint8_t *)frame + bytesRead, m_audioBuffer + m_readPos, firstChunkSize);

        m_readPos = (m_readPos + firstChunkSize) % AUDIO_BUFFER_SIZE;
        m_bufferFilled -= firstChunkSize;
        bytesRead += firstChunkSize;

        // Handle buffer wrap-around
        if (firstChunkSize < chunkSize)
        {
            size_t secondChunkSize = chunkSize - firstChunkSize;
            memcpy((uint8_t *)frame + bytesRead, m_audioBuffer + m_readPos, secondChunkSize);

            m_readPos = (m_readPos + secondChunkSize) % AUDIO_BUFFER_SIZE;
            m_bufferFilled -= secondChunkSize;
            bytesRead += secondChunkSize;
        }
    }

    m_totalBufferReadPos += bytesRead;
    m_bytesPlayed += bytesRead;

    fillBuffer(); // Refill the buffer after reading

    // Update playback status and time
    m_isAudioPlaying = (bytesRead > 0 || m_bufferFilled > 0);

    if (m_muted)
    {
        memset(frame, 0, frame_count * sizeof(Frame)); // Mute audio if necessary
    }

    // Call the frames provided callback if set
    if (m_audioFramesProvidedCallback)
    {
        m_audioFramesProvidedCallback(m_currentPlayingFilePath, frame, frame_count);
    }

    // Check for and handle file transitions
    if (m_totalBufferReadPos >= m_fileStartBufferPos)
    {
        m_playbackStartTime = millis();
        m_currentPlayingFilePath = m_fileStartPath;
        m_bytesPlayed = 0; // Reset m_bytesPlayed to zero when starting a new file

        if (m_playbackStartCallback)
        {
            m_playbackStartCallback(m_fileStartPath);
        }
        // Keep: for debuug: Serial.printf("AudioPlayer::provideAudioFrames() FOUND FILE START MARKER: m_totalBufferReadPos (%zu) >= m_fileStartBufferPos (%zu), starting playback of file: %s\n", m_totalBufferReadPos, m_fileStartBufferPos, m_fileStartPath.c_str());
        m_fileStartBufferPos = BUFFER_POS_UNDEFINED;
        m_fileStartPath = "";
    }

    if (m_totalBufferReadPos >= m_fileEndBufferPos)
    {
        if (m_playbackEndCallback)
        {
            m_playbackEndCallback(m_fileEndPath);
        }
        // Keep: for debuug: Serial.printf("AudioPlayer::provideAudioFrames() FOUND FILE END MARKER: m_totalBufferReadPos (%zu) >= m_fileEndBufferPos (%zu), ending playback of file: %s\n", m_totalBufferReadPos, m_fileEndBufferPos, m_fileEndPath.c_str());
        m_fileEndBufferPos = BUFFER_POS_UNDEFINED;
        m_fileEndPath = "";
    }

    return frame_count;
}

// Fill the audio buffer with data from the current file or start the next file
void AudioPlayer::fillBuffer()
{
    while (m_bufferFilled < AUDIO_BUFFER_SIZE)
    {
        if (!audioFile || !audioFile.available())
        {
            if (audioFile)
            {
                // Add end-of-file transition for the current file
                m_fileEndBufferPos = m_totalBufferWritePos;
                m_fileEndPath = m_currentBufferingFilePath;
                // Keep: for debuug: Serial.printf("AudioPlayer::fillBuffer() ADDING FILE END MARKER: m_fileEndBufferPos: %zu, m_fileEndPath: %s\n", m_fileEndBufferPos, m_fileEndPath.c_str());
                audioFile.close();
            }

            if (!startNextFile())
            {
                break;
            }
        }
        else
        {
            uint8_t audioData[512];
            size_t bytesRead = audioFile.read(audioData, sizeof(audioData));
            if (bytesRead > 0)
            {
                writeToBuffer(audioData, bytesRead);
            }
            else
            {
                // Handle unexpected end of file

                // Add end-of-file transition for the current file
                m_fileEndBufferPos = m_totalBufferWritePos;
                m_fileEndPath = m_currentBufferingFilePath;
                // Keep: for debuug: Serial.printf("AudioPlayer::fillBuffer() ADDING FILE END MARKER (2): m_fileEndBufferPos: %zu, m_fileEndPath: %s\n", m_fileEndBufferPos, m_fileEndPath.c_str());
                audioFile.close();
            }
        }
    }
}

// Write audio data to the circular buffer
void AudioPlayer::writeToBuffer(const uint8_t *audioData, size_t dataSize)
{
    // The circular buffer allows for continuous writing and reading without shuffling data.
    // When the end of the buffer is reached, it wraps around to the beginning.

    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;

    // Write audio data if available
    if (audioData && dataSize > 0 && spaceAvailable > 0)
    {
        size_t bytesToWrite = std::min(dataSize, spaceAvailable);
        size_t firstChunkSize = std::min(bytesToWrite, AUDIO_BUFFER_SIZE - m_writePos);
        memcpy(m_audioBuffer + m_writePos, audioData, firstChunkSize);
        m_writePos = (m_writePos + firstChunkSize) % AUDIO_BUFFER_SIZE;
        m_bufferFilled += firstChunkSize;

        // Handle buffer wrap-around
        if (firstChunkSize < bytesToWrite)
        {
            size_t secondChunkSize = bytesToWrite - firstChunkSize;
            memcpy(m_audioBuffer, audioData + firstChunkSize, secondChunkSize);
            m_writePos = secondChunkSize;
            m_bufferFilled += secondChunkSize;
        }

        m_totalBufferWritePos += bytesToWrite;
    }
}

// Start buffering the next file in the queue
bool AudioPlayer::startNextFile()
{
    if (audioFile)
    {
        audioFile.close();
    }

    if (audioQueue.empty())
    {
        m_currentBufferingFilePath = "";
        m_bytesPlayed = 0; // Reset byte counter to avoid overflows
        return false;
    }

    std::string nextFile = audioQueue.front();
    audioQueue.pop(); // Remove the file from the queue after retrieving it

    audioFile = m_sdCardManager.openFile(nextFile.c_str());
    if (!audioFile)
    {
        Serial.printf("AudioPlayer::startNextFile() Failed to open audio file: %s\n", nextFile.c_str());
        return startNextFile(); // Try the next file in the queue
    }

    // Skip WAV header (simplified approach)
    // 44 bytes is minimum, the skull files have closer to 128 bytes, and skipping a bit more just skips some blank audio at the start.
    // Not skipping enough header will cause the header to be played, often rewsulting in a click at the start of the audio.
    // There's a proper way to parse out the header, but it's involved and this is good enough.
    audioFile.seek(128);

    m_currentBufferingFilePath = String(nextFile.c_str());
    m_fileStartBufferPos = m_totalBufferWritePos;
    m_fileStartPath = m_currentBufferingFilePath;
    // Keep: for debuug: Serial.printf("AudioPlayer::startNextFile() ADDING FILE START MARKER: m_fileStartBufferPos: %zu, m_fileStartPath: %s\n", m_fileStartBufferPos, m_fileStartPath.c_str());
    return true;
}

// Set the muted state of the audio player
void AudioPlayer::setMuted(bool muted)
{
    m_muted = muted;
}

// Check if audio is currently playing
bool AudioPlayer::isAudioPlaying() const
{
    return m_isAudioPlaying;
}

// Get the current playback time based on bytes played
// Calculated based on bytes played and not wall clock time becasue various things like the speed of the
// calls to provideAudioFrames, playback rate, latency, etc can cause the wall clock time to be inaccurate.
// Basing it on the actual bytes read adjusts for a lot of these issues.
unsigned long AudioPlayer::getPlaybackTime() const
{
    if (!m_isAudioPlaying)
    {
        return 0;
    }

    double secondsPlayed = static_cast<double>(m_bytesPlayed) / AUDIO_BYTES_PER_SECOND;

    return static_cast<unsigned long>(secondsPlayed * 1000.0);
}

// Get the file path of the currently playing audio
String AudioPlayer::getCurrentlyPlayingFilePath() const
{
    return m_currentPlayingFilePath;
}