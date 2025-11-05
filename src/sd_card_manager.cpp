#include "sd_card_manager.h"
#include "infra/log_sink.h"
#include "SD_MMC.h"
#include "sdmmc_cmd.h"
#include <Arduino.h>
#include <cstdint>

static constexpr const char* TAG = "SDCard";
static constexpr const char* SD_MOUNT_POINT = "/sdcard";
static constexpr int SD_PIN_CLK = 14;
static constexpr int SD_PIN_CMD = 15;
static constexpr int SD_PIN_D0 = 2;
static constexpr uint32_t SD_MOUNT_FREQUENCY = 20000000UL;

SDCardManager::SDCardManager() {}

bool SDCardManager::begin() {
    // Ensure internal pull-ups on the SDMMC data lines; some dev boards omit external resistors.
    pinMode(SD_PIN_CMD, INPUT_PULLUP);
    pinMode(SD_PIN_D0, INPUT_PULLUP);

    // Explicitly bind the built-in SDMMC pins for the ESP32-WROVER slot.
    SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);

    // Use 1-bit mode (second arg = true) with a reduced clock to maximize compatibility.
    if (!SD_MMC.begin(SD_MOUNT_POINT, true, false, SD_MOUNT_FREQUENCY)) {
        infra::emitLog(infra::LogLevel::Error, TAG,
                       "SD_MMC mount failed (1-bit mode, %lu Hz). Check card seating and slot.",
                       static_cast<unsigned long>(SD_MOUNT_FREQUENCY));
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        infra::emitLog(infra::LogLevel::Error, TAG, "No SD card detected after mount.");
        return false;
    }

    const char* cardTypeStr = "UNKNOWN";
    switch (cardType) {
        case CARD_MMC:
            cardTypeStr = "MMC";
            break;
        case CARD_SD:
            cardTypeStr = "SDSC";
            break;
        case CARD_SDHC:
            cardTypeStr = "SDHC/SDXC";
            break;
        default:
            break;
    }

    uint64_t cardSizeMb = SD_MMC.cardSize() / (1024ULL * 1024ULL);
    infra::emitLog(infra::LogLevel::Info, TAG,
                   "Mounted successfully (%s, %llu MB card size, %lu Hz bus)",
                   cardTypeStr,
                   static_cast<unsigned long long>(cardSizeMb),
                   static_cast<unsigned long>(SD_MOUNT_FREQUENCY));
    return true;
}

SDCardContent SDCardManager::loadContent() {
    SDCardContent content;

    processSkitFiles(content);

    return content;
}

bool SDCardManager::processSkitFiles(SDCardContent& content) {
    File root = SD_MMC.open("/audio");
    if (!root || !root.isDirectory()) {
        infra::emitLog(infra::LogLevel::Error, TAG, "Failed to open /audio directory");
        return false;
    }

    std::vector<String> skitFiles;
    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith("Skit") && fileName.endsWith(".wav")) {
            skitFiles.push_back(fileName);
        }
        file = root.openNextFile();
    }

    infra::emitLog(infra::LogLevel::Info, TAG, "Processing %u skits", static_cast<unsigned>(skitFiles.size()));
    for (const auto& fileName : skitFiles) {
        String baseName = fileName.substring(0, fileName.lastIndexOf('.'));
        String txtFileName = baseName + ".txt";
        String fullWavPath = constructValidPath("/audio", fileName);
        String fullTxtPath = constructValidPath("/audio", txtFileName);

        if (fileExists(fullTxtPath.c_str())) {
            ParsedSkit parsedSkit = parseSkitFile(fullWavPath, fullTxtPath);
            content.skits.push_back(parsedSkit);
            infra::emitLog(infra::LogLevel::Info, TAG, "Processed skit '%s' (%u lines)",
                           fileName.c_str(), static_cast<unsigned>(parsedSkit.lines.size()));
        } else {
            infra::emitLog(infra::LogLevel::Warn, TAG, "Skit '%s' missing txt file", fileName.c_str());
        }
        content.audioFiles.push_back(fullWavPath);
    }

    root.close();
    return true;
}

ParsedSkit SDCardManager::parseSkitFile(const String& wavFile, const String& txtFile) {
    ParsedSkit parsedSkit;
    parsedSkit.audioFile = wavFile;
    parsedSkit.txtFile = txtFile;

    File file = openFile(txtFile.c_str());
    if (!file) {
        infra::emitLog(infra::LogLevel::Error, TAG, "Failed to open skit file: %s", txtFile.c_str());
        return parsedSkit;
    }

    size_t lineNumber = 0;
    while (file.available()) {
        String line = readLine(file);
        line.trim();
        if (line.length() == 0) continue;

        ParsedSkitLine skitLine;
        int commaIndex1 = line.indexOf(',');
        int commaIndex2 = line.indexOf(',', commaIndex1 + 1);
        int commaIndex3 = line.indexOf(',', commaIndex2 + 1);

        skitLine.lineNumber = lineNumber++;
        skitLine.speaker = line.charAt(0);
        skitLine.timestamp = line.substring(commaIndex1 + 1, commaIndex2).toInt();
        skitLine.duration = line.substring(commaIndex2 + 1, commaIndex3).toInt();

        if (commaIndex3 != -1) {
            skitLine.jawPosition = line.substring(commaIndex3 + 1).toFloat();
        } else {
            skitLine.jawPosition = -1;  // Indicating dynamic jaw movement
        }

        parsedSkit.lines.push_back(skitLine);
    }

    file.close();
    return parsedSkit;
}

ParsedSkit SDCardManager::findSkitByName(const std::vector<ParsedSkit>& skits, const String& name) {
    for (const auto& skit : skits) {
        if (skit.audioFile.endsWith(name + ".wav")) {
            return skit;
        }
    }
    return ParsedSkit(); // Return an empty ParsedSkit if not found
}

bool SDCardManager::fileExists(const char* path) {
    File file = SD_MMC.open(path);
    if (!file || file.isDirectory()) {
        return false;
    }
    file.close();
    return true;
}

File SDCardManager::openFile(const char* path) {
    return SD_MMC.open(path);
}

String SDCardManager::readLine(File& file) {
    return file.readStringUntil('\n');
}

size_t SDCardManager::readFileBytes(File& file, uint8_t* buffer, size_t bufferSize) {
    return file.read(buffer, bufferSize);
}

String SDCardManager::constructValidPath(const String& basePath, const String& fileName) {
    String result = basePath;
    
    // Ensure base path ends with a separator
    if (result.length() > 0 && result.charAt(result.length() - 1) != '/') {
        result += '/';
    }
    
    // Append fileName without modification
    result += fileName;
    
    return result;
}

bool SDCardManager::isValidPathChar(char c) {
    // Allow alphanumeric characters, underscore, hyphen, and period
    return isalnum(c) || c == '_' || c == '-' || c == '.';
}
