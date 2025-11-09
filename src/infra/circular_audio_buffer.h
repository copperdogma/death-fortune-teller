#ifndef INFRA_CIRCULAR_AUDIO_BUFFER_H
#define INFRA_CIRCULAR_AUDIO_BUFFER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace infra {

template <std::size_t Capacity>
class CircularAudioBuffer {
public:
    static constexpr std::size_t kCapacity = Capacity;

    CircularAudioBuffer() = default;

    std::size_t capacity() const {
        return kCapacity;
    }

    std::size_t available() const {
        return m_filled;
    }

    std::size_t freeSpace() const {
        return kCapacity - m_filled;
    }

    std::size_t totalWritten() const {
        return m_totalWritten;
    }

    std::size_t totalRead() const {
        return m_totalRead;
    }

    void clear() {
        m_readPos = 0;
        m_writePos = 0;
        m_filled = 0;
        m_totalWritten = 0;
        m_totalRead = 0;
    }

    std::size_t write(const uint8_t *data, std::size_t length) {
        if (!data || length == 0) {
            return 0;
        }

        std::size_t space = freeSpace();
        if (space == 0) {
            return 0;
        }

        std::size_t bytesToWrite = (length > space) ? space : length;
        std::size_t firstChunk = kCapacity - m_writePos;
        if (firstChunk > bytesToWrite) {
            firstChunk = bytesToWrite;
        }

        std::memcpy(m_storage.data() + m_writePos, data, firstChunk);

        std::size_t bytesWritten = firstChunk;
        m_writePos = (m_writePos + firstChunk) % kCapacity;

        if (bytesWritten < bytesToWrite) {
            std::size_t secondChunk = bytesToWrite - bytesWritten;
            std::memcpy(m_storage.data() + m_writePos, data + bytesWritten, secondChunk);
            m_writePos = (m_writePos + secondChunk) % kCapacity;
            bytesWritten += secondChunk;
        }

        m_filled += bytesWritten;
        m_totalWritten += bytesWritten;
        return bytesWritten;
    }

    std::size_t read(uint8_t *dest, std::size_t length, bool padWithSilence, bool forceSilence) {
        if (!dest || length == 0) {
            return 0;
        }

        std::size_t availableBytes = available();
        std::size_t bytesToRead = (length > availableBytes) ? availableBytes : length;
        std::size_t bytesRead = 0;

        if (bytesToRead > 0) {
            std::size_t firstChunk = kCapacity - m_readPos;
            if (firstChunk > bytesToRead) {
                firstChunk = bytesToRead;
            }

            std::memcpy(dest, m_storage.data() + m_readPos, firstChunk);
            bytesRead = firstChunk;
            m_readPos = (m_readPos + firstChunk) % kCapacity;

            if (bytesRead < bytesToRead) {
                std::size_t secondChunk = bytesToRead - bytesRead;
                std::memcpy(dest + bytesRead, m_storage.data() + m_readPos, secondChunk);
                m_readPos = (m_readPos + secondChunk) % kCapacity;
                bytesRead += secondChunk;
            }

            m_filled -= bytesRead;
            m_totalRead += bytesRead;
        }

        if (padWithSilence && bytesRead < length) {
            std::memset(dest + bytesRead, 0, length - bytesRead);
        }

        if (forceSilence) {
            std::memset(dest, 0, length);
        }

        return bytesRead;
    }

private:
    std::array<uint8_t, kCapacity> m_storage{};
    std::size_t m_readPos = 0;
    std::size_t m_writePos = 0;
    std::size_t m_filled = 0;
    std::size_t m_totalWritten = 0;
    std::size_t m_totalRead = 0;
};

}  // namespace infra

#endif  // INFRA_CIRCULAR_AUDIO_BUFFER_H
