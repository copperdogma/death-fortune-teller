#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "SD.h"
#include "sd_card_manager.h"
#include "SoundData.h" // For Frame definition
#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <stdint.h>
#include <Arduino.h>

// AudioPlayer class manages audio playback from SD card files
class AudioPlayer
{
public:
    // Constructor initializes the AudioPlayer with SDCardManager
    AudioPlayer(SDCardManager &sdCardManager);

    // Add a new audio file to the playback queue
    void playNext(String filePath);

    // Provide audio frames to the audio output stream
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);

    // Check if audio is currently playing
    bool isAudioPlaying() const;

    // Set the muted state of the audio player
    void setMuted(bool muted);

    // Get the current playback time
    unsigned long getPlaybackTime() const;

    // Get the file path of the currently playing audio
    String getCurrentlyPlayingFilePath() const;

    // Callback types
    typedef void (*PlaybackCallback)(const String &filePath);
    typedef void (*AudioFramesProvidedCallback)(const String &, const Frame *, int32_t);

    // Setters for callbacks
    void setPlaybackStartCallback(PlaybackCallback callback) { m_playbackStartCallback = callback; }
    void setPlaybackEndCallback(PlaybackCallback callback) { m_playbackEndCallback = callback; }
    void setAudioFramesProvidedCallback(AudioFramesProvidedCallback callback) { m_audioFramesProvidedCallback = callback; }

private:
    static constexpr const char *IDENTIFIER = "AudioPlayer";
    static constexpr size_t BUFFER_POS_UNDEFINED = static_cast<size_t>(-1);
    static constexpr size_t AUDIO_BUFFER_SIZE = 8192; // Size of the circular audio buffer

    // Hardcoded audio format specifications
    static constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;
    static constexpr uint8_t AUDIO_BIT_DEPTH = 16;
    static constexpr uint8_t AUDIO_NUM_CHANNELS = 2;
    static constexpr double AUDIO_BYTES_PER_SECOND = AUDIO_SAMPLE_RATE * (AUDIO_BIT_DEPTH / 8.0) * AUDIO_NUM_CHANNELS;

    // Fill the audio buffer with data from the current file or start the next file
    void fillBuffer();

    // Start playing the next file in the queue
    bool startNextFile();

    // Write audio data to the circular buffer
    void writeToBuffer(const uint8_t *audioData, size_t dataSize);

    // Buffer management
    String m_currentBufferingFilePath;
    uint8_t m_audioBuffer[AUDIO_BUFFER_SIZE];
    size_t m_writePos;
    size_t m_readPos;
    size_t m_bufferFilled;
     
    // Total number of bytes filled in the buffer since start
    size_t m_totalBufferWritePos;
    size_t m_totalBufferReadPos;

    // Playback state
    File audioFile;
    String m_currentPlayingFilePath;
    bool m_isAudioPlaying;
    bool m_muted;

    // Timing
    unsigned long m_playbackStartTime = 0;

    // Audio queue
    std::queue<std::string> audioQueue;

    // SD card manager
    SDCardManager &m_sdCardManager;

    // Thread safety
    std::mutex m_mutex;

    // Callbacks
    PlaybackCallback m_playbackStartCallback;
    PlaybackCallback m_playbackEndCallback;
    AudioFramesProvidedCallback m_audioFramesProvidedCallback;

    size_t m_fileStartBufferPos;
    String m_fileStartPath;
    size_t m_fileEndBufferPos;
    String m_fileEndPath;

    size_t m_bytesPlayed;  // Total bytes played for the current file

    // New method to reset byte counters
    void resetByteCounters();
};

#endif // AUDIO_PLAYER_H