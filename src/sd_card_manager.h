#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include "FS.h"
#include <vector>
#include "parsed_skit.h"

struct SDCardContent {
    std::vector<ParsedSkit> skits;
    std::vector<String> audioFiles;
};

class SDCardManager {
public:
    SDCardManager();
    bool begin();
    SDCardContent loadContent();
    ParsedSkit findSkitByName(const std::vector<ParsedSkit>& skits, const String& name);
    bool fileExists(const char* path);
    File openFile(const char* path);
    String readLine(File& file);
    size_t readFileBytes(File& file, uint8_t* buffer, size_t bufferSize);
    String constructValidPath(const String& basePath, const String& fileName);

private:
    bool processSkitFiles(SDCardContent& content);
    ParsedSkit parseSkitFile(const String& wavFile, const String& txtFile);
    bool isValidPathChar(char c);
};

#endif // SD_CARD_MANAGER_H
