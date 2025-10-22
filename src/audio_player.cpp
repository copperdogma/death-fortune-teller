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

#include <Arduino.h>
#include <algorithm>
#include <cstring>

constexpr size_t AudioPlayer::BUFFER_POS_UNDEFINED;

AudioPlayer::AudioPlayer(SDCardManager &sdCardManager)
    : m_currentBufferingFilePath(""),
      m_writePos(0),
      m_readPos(0),
      m_bufferFilled(0),
      m_totalBufferWritePos(0),
      m_totalBufferReadPos(0),
      m_fileStartBufferPos(BUFFER_POS_UNDEFINED),
      m_fileStartPath(""),
      m_fileEndBufferPos(BUFFER_POS_UNDEFINED),
      m_fileEndPath(""),
      m_isAudioPlaying(false),
      m_muted(false),
      m_sdCardManager(sdCardManager),
      m_pendingStartEvent(false),
      m_pendingEndEvent(false),
      m_bytesPlayed(0)
{
    m_bufferMux = portMUX_INITIALIZER_UNLOCKED;
    m_queueMux = portMUX_INITIALIZER_UNLOCKED;
    m_playbackStartCallback = nullptr;
    m_playbackEndCallback = nullptr;
    m_audioFramesProvidedCallback = nullptr;
}

bool AudioPlayer::hasQueuedAudio()
{
    bool hasAudio = false;
    portENTER_CRITICAL(&m_queueMux);
    hasAudio = !audioQueue.empty();
    portEXIT_CRITICAL(&m_queueMux);
    return hasAudio;
}

void AudioPlayer::playNext(String filePath)
{
    if (filePath.length() == 0)
    {
        return;
    }

    std::string path(filePath.c_str());

    portENTER_CRITICAL(&m_queueMux);
    audioQueue.push(path);
    portEXIT_CRITICAL(&m_queueMux);

    Serial.printf("AudioPlayer::playNext() Added file to queue: %s ...\n", filePath.c_str());
}

int32_t IRAM_ATTR AudioPlayer::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    if (frame_count <= 0 || frame == nullptr)
    {
        return 0;
    }

    const size_t bytesRequested = static_cast<size_t>(frame_count) * sizeof(Frame);
    size_t bytesCopied = 0;
    bool muted = false;

    portENTER_CRITICAL_ISR(&m_bufferMux);

    size_t available = m_bufferFilled;
    if (available == 0)
    {
        m_isAudioPlaying = false;
        muted = m_muted;
        portEXIT_CRITICAL_ISR(&m_bufferMux);

        if (bytesRequested > 0)
        {
            memset(frame, 0, bytesRequested);
        }
        return frame_count;
    }

    size_t bytesToRead = bytesRequested;
    if (bytesToRead > available)
    {
        bytesToRead = available;
    }

    size_t readPos = m_readPos;

    size_t firstChunkSize = AUDIO_BUFFER_SIZE - readPos;
    if (firstChunkSize > bytesToRead)
    {
        firstChunkSize = bytesToRead;
    }

    memcpy(reinterpret_cast<uint8_t *>(frame), m_audioBuffer + readPos, firstChunkSize);

    readPos = (readPos + firstChunkSize) % AUDIO_BUFFER_SIZE;
    available -= firstChunkSize;
    bytesCopied = firstChunkSize;

    if (bytesCopied < bytesToRead)
    {
        size_t secondChunkSize = bytesToRead - bytesCopied;
        memcpy(reinterpret_cast<uint8_t *>(frame) + bytesCopied, m_audioBuffer + readPos, secondChunkSize);
        readPos = (readPos + secondChunkSize) % AUDIO_BUFFER_SIZE;
        available -= secondChunkSize;
        bytesCopied += secondChunkSize;
    }

    m_readPos = readPos;
    m_bufferFilled = available;
    m_totalBufferReadPos += bytesCopied;
    m_bytesPlayed += bytesCopied;

    if (m_fileStartBufferPos != BUFFER_POS_UNDEFINED && m_totalBufferReadPos >= m_fileStartBufferPos)
    {
        m_pendingStartEvent = true;
        m_fileStartBufferPos = BUFFER_POS_UNDEFINED;
    }

    if (m_fileEndBufferPos != BUFFER_POS_UNDEFINED && m_totalBufferReadPos >= m_fileEndBufferPos)
    {
        m_pendingEndEvent = true;
        m_fileEndBufferPos = BUFFER_POS_UNDEFINED;
    }

    m_isAudioPlaying = (bytesCopied > 0) || (m_bufferFilled > 0);
    muted = m_muted;

    portEXIT_CRITICAL_ISR(&m_bufferMux);

    if (bytesCopied < bytesRequested)
    {
        memset(reinterpret_cast<uint8_t *>(frame) + bytesCopied, 0, bytesRequested - bytesCopied);
    }

    if (muted)
    {
        memset(frame, 0, bytesRequested);
    }

    return frame_count;
}

int32_t AudioPlayer::provideAudioData(uint8_t *data, uint32_t len)
{
    int32_t frame_count = len / (2 * sizeof(int16_t)); // 2 channels, 16-bit samples
    Frame *frames = reinterpret_cast<Frame *>(data);
    return provideAudioFrames(frames, frame_count);
}

void AudioPlayer::update()
{
    handlePendingEvents();
    fillBuffer();
    handlePendingEvents();
}

void AudioPlayer::fillBuffer()
{
    while (true)
    {
        size_t bufferFilledSnapshot = 0;
        portENTER_CRITICAL(&m_bufferMux);
        bufferFilledSnapshot = m_bufferFilled;
        portEXIT_CRITICAL(&m_bufferMux);

        if (bufferFilledSnapshot >= AUDIO_BUFFER_SIZE)
        {
            break;
        }

        if (!audioFile || !audioFile.available())
        {
            if (audioFile)
            {
                audioFile.close();
                portENTER_CRITICAL(&m_bufferMux);
                m_fileEndBufferPos = m_totalBufferWritePos;
                m_fileEndPath = m_currentBufferingFilePath;
                portEXIT_CRITICAL(&m_bufferMux);
            }

            if (!startNextFile())
            {
                break;
            }
            continue;
        }

        uint8_t audioData[512];
        size_t bytesRead = audioFile.read(audioData, sizeof(audioData));
        if (bytesRead > 0)
        {
            writeToBuffer(audioData, bytesRead);
        }
        else
        {
            audioFile.close();
            portENTER_CRITICAL(&m_bufferMux);
            m_fileEndBufferPos = m_totalBufferWritePos;
            m_fileEndPath = m_currentBufferingFilePath;
            portEXIT_CRITICAL(&m_bufferMux);
        }
    }
}

void AudioPlayer::writeToBuffer(const uint8_t *audioData, size_t dataSize)
{
    if (!audioData || dataSize == 0)
    {
        return;
    }

    portENTER_CRITICAL(&m_bufferMux);

    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
    if (spaceAvailable == 0)
    {
        portEXIT_CRITICAL(&m_bufferMux);
        return;
    }

    size_t bytesToWrite = dataSize;
    if (bytesToWrite > spaceAvailable)
    {
        bytesToWrite = spaceAvailable;
    }

    size_t writePos = m_writePos;
    size_t firstChunkSize = AUDIO_BUFFER_SIZE - writePos;
    if (firstChunkSize > bytesToWrite)
    {
        firstChunkSize = bytesToWrite;
    }

    memcpy(m_audioBuffer + writePos, audioData, firstChunkSize);

    writePos = (writePos + firstChunkSize) % AUDIO_BUFFER_SIZE;
    size_t bytesWritten = firstChunkSize;
    size_t newBufferFilled = m_bufferFilled + firstChunkSize;

    if (bytesWritten < bytesToWrite)
    {
        size_t secondChunkSize = bytesToWrite - bytesWritten;
        memcpy(m_audioBuffer + writePos, audioData + bytesWritten, secondChunkSize);
        writePos = (writePos + secondChunkSize) % AUDIO_BUFFER_SIZE;
        newBufferFilled += secondChunkSize;
        bytesWritten += secondChunkSize;
    }

    m_writePos = writePos;
    m_bufferFilled = newBufferFilled;
    m_totalBufferWritePos += bytesWritten;

    portEXIT_CRITICAL(&m_bufferMux);
}

bool AudioPlayer::startNextFile()
{
    std::string nextFile;

    while (true)
    {
        {
            portENTER_CRITICAL(&m_queueMux);
            if (audioQueue.empty())
            {
                portEXIT_CRITICAL(&m_queueMux);
                m_currentBufferingFilePath = "";
                return false;
            }
            nextFile = audioQueue.front();
            audioQueue.pop();
            portEXIT_CRITICAL(&m_queueMux);
        }

        audioFile = m_sdCardManager.openFile(nextFile.c_str());
        if (!audioFile)
        {
            Serial.printf("AudioPlayer::startNextFile() Failed to open audio file: %s\n", nextFile.c_str());
            continue;
        }

        // Skip WAV header (simplified approach)
        audioFile.seek(128);

        portENTER_CRITICAL(&m_bufferMux);
        m_currentBufferingFilePath = String(nextFile.c_str());
        m_fileStartBufferPos = m_totalBufferWritePos;
        m_fileStartPath = m_currentBufferingFilePath;
        portEXIT_CRITICAL(&m_bufferMux);

        return true;
    }
}

void AudioPlayer::setMuted(bool muted)
{
    portENTER_CRITICAL(&m_bufferMux);
    m_muted = muted;
    portEXIT_CRITICAL(&m_bufferMux);
}

bool AudioPlayer::isAudioPlaying() const
{
    return m_isAudioPlaying;
}

unsigned long AudioPlayer::getPlaybackTime() const
{
    size_t bytesPlayedSnapshot = 0;
    bool isPlayingSnapshot = false;

    portENTER_CRITICAL(&const_cast<AudioPlayer *>(this)->m_bufferMux);
    bytesPlayedSnapshot = m_bytesPlayed;
    isPlayingSnapshot = m_isAudioPlaying;
    portEXIT_CRITICAL(&const_cast<AudioPlayer *>(this)->m_bufferMux);

    if (!isPlayingSnapshot)
    {
        return 0;
    }

    double secondsPlayed = static_cast<double>(bytesPlayedSnapshot) / AUDIO_BYTES_PER_SECOND;
    return static_cast<unsigned long>(secondsPlayed * 1000.0);
}

String AudioPlayer::getCurrentlyPlayingFilePath() const
{
    return m_currentPlayingFilePath;
}

void AudioPlayer::handlePendingEvents()
{
    String startPath;
    String endPath;
    bool startEvent = false;
    bool endEvent = false;

    portENTER_CRITICAL(&m_bufferMux);
    if (m_pendingStartEvent)
    {
        m_pendingStartEvent = false;
        startEvent = true;
        startPath = m_fileStartPath;
        m_fileStartPath = "";
        m_bytesPlayed = 0;
    }

    if (m_pendingEndEvent)
    {
        m_pendingEndEvent = false;
        endEvent = true;
        endPath = m_fileEndPath;
        m_fileEndPath = "";
        if (m_bufferFilled == 0)
        {
            m_isAudioPlaying = false;
        }
    }
    portEXIT_CRITICAL(&m_bufferMux);

    if (startEvent)
    {
        m_currentPlayingFilePath = startPath;
        m_playbackStartTime = millis();
        if (m_playbackStartCallback)
        {
            m_playbackStartCallback(startPath);
        }
    }

    if (endEvent)
    {
        m_currentPlayingFilePath = "";
        if (m_playbackEndCallback)
        {
            m_playbackEndCallback(endPath);
        }
    }
}
