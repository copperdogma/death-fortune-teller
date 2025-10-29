#include "thermal_printer.h"
#include "logging_manager.h"

#include <SD_MMC.h>
#include <algorithm>
#include <memory>

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
      lastCommandTime(0) {
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
}

void ThermalPrinter::update() {
    if (!initialized) {
        return;
    }

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
}

bool ThermalPrinter::printFortune(const String &fortune) {
    if (!initialized) {
        LOG_WARN(TAG, "Thermal printer not initialized; skipping fortune print");
        return false;
    }
    if (hasErrorState) {
        LOG_WARN(TAG, "Thermal printer in error state; skipping fortune print");
        return false;
    }

    LOG_INFO(TAG, "Printing fortune (%u chars)", static_cast<unsigned>(fortune.length()));

    sendInitSequence();

    bool bitmapPrinted = printLogo();
    if (!bitmapPrinted) {
        LOG_WARN(TAG, "Bitmap logo unavailable; printed text fallback instead");
    }

    // Leave a blank line after the logo so the fortune text begins cleanly.
    serial.println();

    setJustification(0);              // Left align for body text
    setDefaultLineSpacing();

    serial.println("Your fortune:");
    serial.println();

    printFortuneBody(fortune);

    serial.println();
    serial.println("--- Death ---");
    serial.println();

    feedLines(3);
    serial.flush();

    lastCommandTime = 0;
    hasErrorState = false;
    return true;
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
    serial.flush();
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

bool ThermalPrinter::printBitmapLogo() {
    if (logoPath.length() == 0) {
        LOG_WARN(TAG, "No printer logo path configured");
        return false;
    }

    File logoFile = SD_MMC.open(logoPath.c_str(), FILE_READ);
    if (!logoFile) {
        LOG_WARN(TAG, "Printer logo file not found: %s", logoPath.c_str());
        return false;
    }

    bool success = printBitmapFromFile(logoFile);
    logoFile.close();
    if (success) {
        LOG_INFO(TAG, "Bitmap logo printed successfully");
    }
    return success;
}

bool ThermalPrinter::printBitmapFromFile(File &file) {
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

    // Determine palette order to know whether to invert bits (ESC/POS expects 1=black).
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

    return streamBitmap(file, width, height, dataOffset, invertBits);
}

bool ThermalPrinter::streamBitmap(File &file, int32_t width, int32_t height, uint32_t dataOffset, bool invert) {
    int32_t absWidth = width < 0 ? -width : width;
    int32_t absHeight = height < 0 ? -height : height;
    bool bottomUp = height > 0;

    const uint16_t payloadBytes = static_cast<uint16_t>((absWidth + 7) / 8);
    const uint16_t strideBytes = static_cast<uint16_t>(((payloadBytes + 3) / 4) * 4); // rows padded to 4-byte boundary
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

    const uint8_t mode = 0x00; // Single-density, normal
    const uint16_t sendRowBytes = padBytes + payloadBytes;

    // ESC/POS raster bitmap header: GS v 0 m xL xH yL yH
    serial.write(0x1D);
    serial.write('v');
    serial.write('0');
    serial.write(mode);
    serial.write(static_cast<uint8_t>(sendRowBytes & 0xFF));
    serial.write(static_cast<uint8_t>((sendRowBytes >> 8) & 0xFF));
    serial.write(static_cast<uint8_t>(absHeight & 0xFF));
    serial.write(static_cast<uint8_t>((absHeight >> 8) & 0xFF));

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
            serial.write(static_cast<uint8_t>(0x00));
        }
        serial.write(lineBuffer.get(), payloadBytes);
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

void ThermalPrinter::printWrappedLine(const String &line) {
    if (line.length() == 0) {
        serial.println();
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
        serial.println(segment);

        start += length;
        while (start < line.length() && line.charAt(start) == ' ') {
            ++start;
        }
    }
}

void ThermalPrinter::printFortuneBody(const String &fortune) {
    if (fortune.length() == 0) {
        serial.println("[No fortune available]");
        return;
    }

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
            serial.println();
            continue;
        }
        printWrappedLine(trimmed);
    }
}
