#include "sd_card_manager.h"

SDCardManager::SDCardManager() {}

bool SDCardManager::begin() {
    if (!SD.begin()) {
        Serial.println("SD Card: Mount Failed!");
        return false;
    }
    Serial.println("SD Card: Mounted successfully");
    return true;
}

SDCardContent SDCardManager::loadContent() {
    SDCardContent content;

    // Check for initialization files
    if (fileExists("/audio/Initialized - Primary.wav")) {
        content.primaryInitAudio = "/audio/Initialized - Primary.wav";
    }
    if (fileExists("/audio/Initialized - Secondary.wav")) {
        content.secondaryInitAudio = "/audio/Initialized - Secondary.wav";
    }

    Serial.println("Required file '/audio/Initialized - Primary.wav' " + 
        String(fileExists("/audio/Initialized - Primary.wav") ? "found." : "missing."));
    Serial.println("Required file '/audio/Initialized - Secondary.wav' " + 
        String(fileExists("/audio/Initialized - Secondary.wav") ? "found." : "missing."));

    processSkitFiles(content);

    return content;
}

bool SDCardManager::processSkitFiles(SDCardContent& content) {
    File root = SD.open("/audio");
    if (!root || !root.isDirectory()) {
        Serial.println("SD Card: Failed to open /audio directory");
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

    Serial.println("Processing " + String(skitFiles.size()) + " skits:");
    for (const auto& fileName : skitFiles) {
        String baseName = fileName.substring(0, fileName.lastIndexOf('.'));
        String txtFileName = baseName + ".txt";
        String fullWavPath = constructValidPath("/audio", fileName);
        String fullTxtPath = constructValidPath("/audio", txtFileName);

        if (fileExists(fullTxtPath.c_str())) {
            ParsedSkit parsedSkit = parseSkitFile(fullWavPath, fullTxtPath);
            content.skits.push_back(parsedSkit);
            Serial.println("- Processing skit '" + fileName + "' - success. (" + String(parsedSkit.lines.size()) + " lines)");
        } else {
            Serial.println("- Processing skit '" + fileName + "' - WARNING: missing txt file.");
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
        Serial.println("Failed to open skit file: " + txtFile);
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
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        return false;
    }
    file.close();
    return true;
}

File SDCardManager::openFile(const char* path) {
    return SD.open(path);
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