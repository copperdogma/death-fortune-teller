#include "thermal_printer.h"
#include "logging_manager.h"

#include <SD_MMC.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace {
constexpr size_t BMP_HEADER_BYTES = 54;

uint16_t readLE16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(data[1]) << 8;
}

uint32_t readLE32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

int luminance(uint8_t b, uint8_t g, uint8_t r) {
    // Approximate luminance without floating point (BT.601 weights)
    return static_cast<int>(r) * 30 + static_cast<int>(g) * 59 + static_cast<int>(b) * 11;
}
} // namespace

static constexpr const char *TAG = "ThermalPrinter";

ThermalPrinter::ThermalPrinter(HardwareSerial &serialPort, int txPin, int rxPin, int baud)
    : serial(serialPort),
      txPin(txPin),
      rxPin(rxPin),
      printerBaud((baud < 1200 || baud > 115200) ? 9600 : baud),
      logoPath(),
      initialized(false),
      hasErrorState(false),
      lastCommandTime(0),
      jobStage(PrintJobStage::Idle),
      pendingFortune(),
      fortuneLines(),
      fortuneLineIndex(0),
      lineCharIndex(0),
      feedLinesRemaining(0),
      logoCache(),
      logoCacheValid(false),
      logoCacheOffset(0),
      logoFallbackPending(false),
      fortuneBodyLineCount(0),
      lastSerialWriteMs(0) {
}

void ThermalPrinter::begin() {
    serial.begin(printerBaud, SERIAL_8N1, rxPin, txPin);
    delay(50);
    initialized = true;
    hasErrorState = false;
    sendInitSequence();
    LOG_INFO(TAG, "Thermal printer initialized at %d baud (TX=%d RX=%d)",
             printerBaud,
             txPin,
             rxPin);
    resetPrintJob();
}

void ThermalPrinter::update() {
    if (!initialized) {
        return;
    }

    processPrintJob();

    if (lastCommandTime > 0 && millis() - lastCommandTime > COMMAND_TIMEOUT_MS) {
        if (!hasErrorState) {
            handleError();
        }
        lastCommandTime = 0;
    }
}

void ThermalPrinter::setLogoPath(const String &path) {
    String trimmed = path;
    trimmed.trim();
    logoPath = trimmed;
    if (logoPath.length() == 0) {
        LOG_WARN(TAG, "Printer logo path cleared; fallback text logo will be used");
    } else {
        LOG_INFO(TAG, "Printer logo path set to %s", logoPath.c_str());
    }
    logoCacheValid = false;
    logoCache.clear();
    if (logoPath.length() != 0) {
        ensureLogoCache();
    }
}

bool ThermalPrinter::printFortune(const String &fortune) {
    return queueFortunePrint(fortune);
}

bool ThermalPrinter::printLogo() {
    if (!initialized) {
        return false;
    }

    setJustification(1);              // Center logo
    setDefaultLineSpacing();

    bool success = printBitmapLogo();
    if (!success) {
        printTextLogoFallback();
    }
    return success;
}

bool ThermalPrinter::queueFortunePrint(const String &fortune) {
    if (!initialized) {
        LOG_WARN(TAG, "Thermal printer not initialized; skipping fortune print");
        return false;
    }
    if (hasErrorState) {
        LOG_WARN(TAG, "Thermal printer in error state; skipping fortune print");
        return false;
    }
    if (jobStage != PrintJobStage::Idle) {
        LOG_WARN(TAG, "Thermal printer already busy with a print job");
        return false;
    }

    pendingFortune = fortune;

    std::vector<String> bodyLines;
    buildFortuneLines(pendingFortune, bodyLines);
    fortuneBodyLineCount = bodyLines.size();

    fortuneLines.clear();
    fortuneLines.emplace_back("Your fortune:");
    fortuneLines.emplace_back("");
    fortuneLines.insert(fortuneLines.end(), bodyLines.begin(), bodyLines.end());
    fortuneLines.emplace_back("");
    fortuneLines.emplace_back("--- Death ---");
    fortuneLines.emplace_back("");

    fortuneLineIndex = 0;
    lineCharIndex = 0;
    feedLinesRemaining = 3;
    logoCacheOffset = 0;
    logoFallbackPending = false;
    lastSerialWriteMs = 0;

    jobStage = PrintJobStage::InitSequence;
    LOG_INFO(TAG, "Queued fortune print job (%u chars, %u lines)",
             static_cast<unsigned>(fortune.length()),
             static_cast<unsigned>(fortuneBodyLineCount));
    return true;
}

bool ThermalPrinter::isPrinting() const {
    return jobStage != PrintJobStage::Idle;
}

void ThermalPrinter::resetPrintJob() {
    jobStage = PrintJobStage::Idle;
    pendingFortune.clear();
    fortuneLines.clear();
    fortuneLineIndex = 0;
    lineCharIndex = 0;
    feedLinesRemaining = 0;
    logoCacheOffset = 0;
    logoFallbackPending = false;
    fortuneBodyLineCount = 0;
    lastSerialWriteMs = 0;
}

bool ThermalPrinter::printTestPage() {
    if (!initialized) {
        LOG_WARN(TAG, "Thermal printer not initialized; cannot print test page");
        return false;
    }
    if (hasErrorState) {
        LOG_WARN(TAG, "Thermal printer in error state; cannot print test page");
        return false;
    }

    sendInitSequence();
    LOG_INFO(TAG, "Triggering printer self-test page");
    serial.write(0x12); // DC2
    serial.write('T');  // 'T' — built-in self-test
    feedLines(3);
    lastCommandTime = 0;
    return true;
}

bool ThermalPrinter::isReady() {
    return initialized && !hasErrorState;
}

bool ThermalPrinter::hasError() {
    return hasErrorState;
}

void ThermalPrinter::sendCommand(uint8_t cmd) {
    if (!initialized) {
        return;
    }
    serial.write(cmd);
}

void ThermalPrinter::sendCommand(uint8_t cmd, uint8_t param) {
    if (!initialized) {
        return;
    }
    serial.write(cmd);
    serial.write(param);
}

void ThermalPrinter::sendCommand(uint8_t cmd, uint8_t *data, size_t length) {
    if (!initialized) {
        return;
    }
    serial.write(cmd);
    serial.write(data, length);
}

bool ThermalPrinter::waitForResponse(unsigned long timeoutMs) {
    unsigned long startTime = millis();
    while (millis() - startTime < timeoutMs) {
        if (serial.available()) {
            return true;
        }
        delay(10);
    }
    return false;
}

void ThermalPrinter::handleError() {
    if (hasErrorState) {
        return;
    }
    hasErrorState = true;
    LOG_ERROR(TAG, "Thermal printer error - check paper, cover, and power");
}

void ThermalPrinter::sendInitSequence() {
    if (!initialized) {
        return;
    }

    serial.write(0x1B); // ESC @ reset
    serial.write('@');
    setDefaultLineSpacing();
    setJustification(0);
}

void ThermalPrinter::setJustification(uint8_t mode) {
    if (!initialized) {
        return;
    }
    uint8_t value = mode > 2 ? 0 : mode; // 0=left,1=center,2=right
    serial.write(0x1B);
    serial.write('a');
    serial.write(value);
}

void ThermalPrinter::setLineSpacing(uint8_t dots) {
    if (!initialized) {
        return;
    }
    serial.write(0x1B);
    serial.write('3');
    serial.write(dots);
}

void ThermalPrinter::setDefaultLineSpacing() {
    if (!initialized) {
        return;
    }
    serial.write(0x1B);
    serial.write('2');
}

void ThermalPrinter::feedLines(uint8_t count) {
    if (!initialized) {
        return;
    }
    for (uint8_t i = 0; i < count; ++i) {
        serial.write('\n');
    }
}

void ThermalPrinter::writeByte(uint8_t byte, std::vector<uint8_t> *buffer) {
    if (buffer) {
        buffer->push_back(byte);
    } else {
        serial.write(byte);
    }
}

void ThermalPrinter::writeData(const uint8_t *data, size_t length, std::vector<uint8_t> *buffer) {
    if (buffer) {
        buffer->insert(buffer->end(), data, data + length);
    } else {
        serial.write(data, length);
    }
}

bool ThermalPrinter::ensureLogoCache() {
    if (logoCacheValid) {
        return true;
    }
    if (logoPath.length() == 0) {
        return false;
    }
    return loadLogoCache();
}

bool ThermalPrinter::loadLogoCache() {
    if (logoPath.length() == 0) {
        return false;
    }

    File file = SD_MMC.open(logoPath.c_str(), FILE_READ);
    if (!file) {
        LOG_WARN(TAG, "Printer logo file not found: %s", logoPath.c_str());
        return false;
    }

    logoCache.clear();
    bool success = processBitmap(file, &logoCache);
    file.close();
    if (success) {
        logoCacheValid = true;
        LOG_INFO(TAG, "Cached printer logo (%u bytes)", static_cast<unsigned>(logoCache.size()));
    } else {
        logoCache.clear();
        logoCacheValid = false;
    }
    return success;
}

bool ThermalPrinter::printBitmapLogo() {
    if (!ensureLogoCache()) {
        LOG_WARN(TAG, "No cached logo available; falling back to text logo");
        return false;
    }

    if (logoCache.empty()) {
        LOG_WARN(TAG, "Cached logo buffer empty; falling back to text logo");
        return false;
    }

    constexpr size_t CHUNK_SIZE = 128;
    size_t offset = 0;
    while (offset < logoCache.size()) {
        size_t remaining = logoCache.size() - offset;
        size_t toWrite = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        serial.write(logoCache.data() + offset, toWrite);
        offset += toWrite;
        delay(0);
    }
    return true;
}

bool ThermalPrinter::printBitmapFromFile(File &file) {
    return processBitmap(file, nullptr);
}

bool ThermalPrinter::processBitmap(File &file, std::vector<uint8_t> *buffer) {
    uint8_t header[BMP_HEADER_BYTES];
    size_t bytesRead = file.read(header, sizeof(header));
    if (bytesRead != sizeof(header)) {
        LOG_ERROR(TAG, "Bitmap header too short (%u bytes)", static_cast<unsigned>(bytesRead));
        return false;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        LOG_ERROR(TAG, "Bitmap signature mismatch (expected BM)");
        return false;
    }

    uint32_t dataOffset = readLE32(header + 10);
    uint32_t dibHeaderSize = readLE32(header + 14);
    if (dibHeaderSize < 40) {
        LOG_ERROR(TAG, "Unsupported BMP DIB header size: %u", static_cast<unsigned>(dibHeaderSize));
        return false;
    }

    int32_t width = static_cast<int32_t>(readLE32(header + 18));
    int32_t height = static_cast<int32_t>(readLE32(header + 22));
    uint16_t planes = readLE16(header + 26);
    uint16_t bitsPerPixel = readLE16(header + 28);
    uint32_t compression = readLE32(header + 30);

    int32_t absWidth = width < 0 ? -width : width;
    int32_t absHeight = height < 0 ? -height : height;

    if (planes != 1) {
        LOG_ERROR(TAG, "Unsupported BMP plane count: %u", planes);
        return false;
    }

    if (compression != 0) {
        LOG_ERROR(TAG, "Compressed BMP logos are not supported (compression=%u)", static_cast<unsigned>(compression));
        return false;
    }

    if (bitsPerPixel != 1) {
        LOG_ERROR(TAG, "Only 1-bit BMP logos are supported (bpp=%u)", static_cast<unsigned>(bitsPerPixel));
        return false;
    }

    if (absWidth <= 0 || absHeight <= 0) {
        LOG_ERROR(TAG, "Invalid BMP dimensions: %ldx%ld", static_cast<long>(width), static_cast<long>(height));
        return false;
    }

    if (absWidth > PRINTER_MAX_WIDTH_DOTS) {
        LOG_ERROR(TAG, "BMP width %ld exceeds printer limit of %u dots", static_cast<long>(absWidth), PRINTER_MAX_WIDTH_DOTS);
        return false;
    }

    if (absHeight > 65535) {
        LOG_ERROR(TAG, "BMP height %ld exceeds ESC/POS limit (65535)", static_cast<long>(absHeight));
        return false;
    }

    bool invertBits = false;
    uint32_t paletteStart = 14 + dibHeaderSize;
    if (!file.seek(paletteStart)) {
        LOG_ERROR(TAG, "Failed to seek to BMP palette");
        return false;
    }

    uint8_t palette[8];
    size_t paletteBytes = file.read(palette, sizeof(palette));
    if (paletteBytes != sizeof(palette)) {
        LOG_ERROR(TAG, "Failed to read BMP palette (%u bytes)", static_cast<unsigned>(paletteBytes));
        return false;
    }

    int lum0 = luminance(palette[0], palette[1], palette[2]);
    int lum1 = luminance(palette[4], palette[5], palette[6]);
    int blackIndex = (lum0 <= lum1) ? 0 : 1;
    if (blackIndex == 0) {
        invertBits = true;
    }

    return streamBitmap(file, width, height, dataOffset, invertBits, buffer);
}

bool ThermalPrinter::streamBitmap(File &file, int32_t width, int32_t height, uint32_t dataOffset, bool invert, std::vector<uint8_t> *buffer) {
    int32_t absWidth = width < 0 ? -width : width;
    int32_t absHeight = height < 0 ? -height : height;
    bool bottomUp = height > 0;

    const uint16_t payloadBytes = static_cast<uint16_t>((absWidth + 7) / 8);
    const uint16_t strideBytes = static_cast<uint16_t>(((payloadBytes + 3) / 4) * 4);
    const uint16_t maxRowBytes = static_cast<uint16_t>((PRINTER_MAX_WIDTH_DOTS + 7) / 8);
    const uint16_t padBytes = (payloadBytes < maxRowBytes) ? static_cast<uint16_t>((maxRowBytes - payloadBytes) / 2) : 0;

    if (!file.seek(dataOffset)) {
        LOG_ERROR(TAG, "Failed to seek to BMP pixel data (offset %lu)", static_cast<unsigned long>(dataOffset));
        return false;
    }

    std::unique_ptr<uint8_t[]> rowBuffer(new (std::nothrow) uint8_t[strideBytes]);
    std::unique_ptr<uint8_t[]> lineBuffer(new (std::nothrow) uint8_t[payloadBytes]);
    if (!rowBuffer || !lineBuffer) {
        LOG_ERROR(TAG, "Insufficient memory for bitmap buffers (%u stride bytes)", static_cast<unsigned>(strideBytes));
        return false;
    }

    const uint8_t mode = 0x00;
    const uint16_t sendRowBytes = padBytes + payloadBytes;

    writeByte(0x1D, buffer);
    writeByte('v', buffer);
    writeByte('0', buffer);
    writeByte(mode, buffer);
    writeByte(static_cast<uint8_t>(sendRowBytes & 0xFF), buffer);
    writeByte(static_cast<uint8_t>((sendRowBytes >> 8) & 0xFF), buffer);
    writeByte(static_cast<uint8_t>(absHeight & 0xFF), buffer);
    writeByte(static_cast<uint8_t>((absHeight >> 8) & 0xFF), buffer);

    const uint8_t remainBitsMask = (absWidth % 8 == 0) ? 0xFF : static_cast<uint8_t>(0xFF << (8 - (absWidth % 8)));

    for (int32_t row = 0; row < absHeight; ++row) {
        int32_t sourceRow = bottomUp ? (absHeight - 1 - row) : row;
        uint32_t offset = dataOffset + static_cast<uint32_t>(sourceRow) * strideBytes;
        if (!file.seek(offset)) {
            LOG_ERROR(TAG, "Failed to seek to BMP row %ld (offset %lu)", static_cast<long>(sourceRow), static_cast<unsigned long>(offset));
            return false;
        }

        size_t rowBytes = file.read(rowBuffer.get(), strideBytes);
        if (rowBytes != strideBytes) {
            LOG_ERROR(TAG, "Failed to read BMP row %ld (%u bytes)", static_cast<long>(sourceRow), static_cast<unsigned>(rowBytes));
            return false;
        }

        for (uint16_t i = 0; i < payloadBytes; ++i) {
            uint8_t value = rowBuffer[i];
            if (invert) {
                value = static_cast<uint8_t>(~value);
            }
            lineBuffer[i] = value;
        }

        if (payloadBytes > 0 && remainBitsMask != 0xFF) {
            lineBuffer[payloadBytes - 1] &= remainBitsMask;
        }

        for (uint16_t i = 0; i < padBytes; ++i) {
            writeByte(0x00, buffer);
        }
        writeData(lineBuffer.get(), payloadBytes, buffer);
    }

    return true;
}

bool ThermalPrinter::printTextLogoFallback() {
    serial.println("******************************");
    serial.println("      DEATH'S FORTUNE");
    serial.println("           TELLER");
    serial.println("******************************");
    return false;
}

void ThermalPrinter::buildFortuneLines(const String &fortune, std::vector<String> &outLines) {
    outLines.clear();
    if (fortune.length() == 0) {
        outLines.emplace_back("[No fortune available]");
        return;
    }

    auto appendWrapped = [this, &outLines](const String &line) {
        if (line.length() == 0) {
            outLines.emplace_back(String());
            return;
        }

        int start = 0;
        const int maxCols = static_cast<int>(MAX_TEXT_COLUMNS);

        while (start < line.length()) {
            int remaining = line.length() - start;
            int length = remaining < maxCols ? remaining : maxCols;

            if (remaining > maxCols) {
                int lastSpace = line.lastIndexOf(' ', start + length);
                if (lastSpace >= start && lastSpace - start <= maxCols) {
                    length = lastSpace - start;
                }
            }

            if (length <= 0) {
                length = std::min(remaining, maxCols);
            }

            String segment = line.substring(start, start + length);
            segment.trim();
            outLines.emplace_back(segment);

            start += length;
            while (start < line.length() && line.charAt(start) == ' ') {
                ++start;
            }
        }
    };

    int lineStart = 0;
    while (lineStart <= fortune.length()) {
        int newlineIndex = fortune.indexOf('\n', lineStart);
        bool hasMore = newlineIndex != -1;
        String line;
        if (hasMore) {
            line = fortune.substring(lineStart, newlineIndex);
            lineStart = newlineIndex + 1;
        } else {
            line = fortune.substring(lineStart);
            lineStart = fortune.length() + 1;
        }

        String trimmed = line;
        trimmed.trim();
        if (trimmed.length() == 0) {
            outLines.emplace_back(String());
            continue;
        }
        appendWrapped(trimmed);
    }
}

void ThermalPrinter::printFortuneBody(const String &fortune) {
    std::vector<String> lines;
    buildFortuneLines(fortune, lines);
    for (const auto &line : lines) {
        if (line.length() == 0) {
            serial.println();
        } else {
            serial.println(line);
        }
    }
}

void ThermalPrinter::processPrintJob() {
    if (jobStage == PrintJobStage::Idle) {
        return;
    }

    auto canWrite = [this]() -> bool {
        unsigned long now = millis();
        if (lastSerialWriteMs != 0 && now - lastSerialWriteMs < SERIAL_WRITE_INTERVAL_MS) {
            return false;
        }
        return serial.availableForWrite() > 0;
    };

    switch (jobStage) {
        case PrintJobStage::InitSequence:
            sendInitSequence();
            jobStage = PrintJobStage::LogoStart;
            break;

        case PrintJobStage::LogoStart:
            setJustification(1);
            setDefaultLineSpacing();
            if (ensureLogoCache() && !logoCache.empty()) {
                logoCacheOffset = 0;
                jobStage = PrintJobStage::LogoRow;
            } else {
                logoFallbackPending = true;
                jobStage = PrintJobStage::LogoComplete;
            }
            break;

        case PrintJobStage::LogoRow: {
            if (logoCacheOffset >= logoCache.size()) {
                jobStage = PrintJobStage::LogoComplete;
                break;
            }
            if (!canWrite()) {
                return;
            }
            size_t writable = serial.availableForWrite();
            if (writable == 0) {
                return;
            }
            size_t remaining = logoCache.size() - logoCacheOffset;
            size_t toWrite = std::min<size_t>(LOGO_CHUNK_SIZE, remaining);
            toWrite = std::min(toWrite, writable);
            serial.write(logoCache.data() + logoCacheOffset, toWrite);
            logoCacheOffset += toWrite;
            lastSerialWriteMs = millis();
            if (logoCacheOffset >= logoCache.size()) {
                jobStage = PrintJobStage::LogoComplete;
            }
            break;
        }

        case PrintJobStage::LogoComplete:
            jobStage = PrintJobStage::BodyHeader;
            break;

        case PrintJobStage::BodyHeader: {
            std::vector<String> prefix;
            if (logoFallbackPending) {
                prefix.emplace_back("******************************");
                prefix.emplace_back("      DEATH'S FORTUNE");
                prefix.emplace_back("           TELLER");
                prefix.emplace_back("******************************");
                logoFallbackPending = false;
            }
            prefix.emplace_back(String());
            fortuneLines.insert(fortuneLines.begin(), prefix.begin(), prefix.end());

            setJustification(0);
            setDefaultLineSpacing();
            fortuneLineIndex = 0;
            lineCharIndex = 0;
            jobStage = PrintJobStage::BodyLine;
            break;
        }

        case PrintJobStage::BodyLine: {
            if (fortuneLineIndex >= fortuneLines.size()) {
                jobStage = PrintJobStage::Feed;
                break;
            }

            unsigned long now = millis();
            if (lastSerialWriteMs != 0 && now - lastSerialWriteMs < SERIAL_WRITE_INTERVAL_MS) {
                return;
            }

            size_t writable = serial.availableForWrite();
            if (writable == 0) {
                return;
            }

            const String &line = fortuneLines[fortuneLineIndex];
            if (lineCharIndex < line.length()) {
                size_t remaining = line.length() - lineCharIndex;
                size_t toWrite = std::min<size_t>(remaining, writable);
                serial.write(reinterpret_cast<const uint8_t *>(line.c_str() + lineCharIndex), toWrite);
                lineCharIndex += toWrite;
                lastSerialWriteMs = now;
                return;
            }

            serial.write('\n');
            lastSerialWriteMs = millis();
            ++fortuneLineIndex;
            lineCharIndex = 0;
            return;
        }

        case PrintJobStage::Feed: {
            if (feedLinesRemaining == 0) {
                jobStage = PrintJobStage::Complete;
                break;
            }

            if (!canWrite()) {
                return;
            }
            if (serial.availableForWrite() == 0) {
                return;
            }

            serial.write('\n');
            lastSerialWriteMs = millis();
            if (feedLinesRemaining > 0) {
                --feedLinesRemaining;
            }
            break;
        }

        case PrintJobStage::Complete:
            LOG_INFO(TAG, "Fortune print job completed (%u body lines)", static_cast<unsigned>(fortuneBodyLineCount));
            resetPrintJob();
            break;

        case PrintJobStage::Idle:
            break;
    }
}
