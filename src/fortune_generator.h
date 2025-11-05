#ifndef FORTUNE_GENERATOR_H
#define FORTUNE_GENERATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include "infra/log_sink.h"

namespace infra {
class IFileSystem;
class IRandomSource;
class ILogSink;
void setLogSink(ILogSink *sink);
ILogSink *getLogSink();
}

struct FortuneTemplate {
    String template_text;
    std::vector<String> tokens;
};

class FortuneGenerator {
public:
    FortuneGenerator();
    bool loadFortunes(const String& filePath);
    void setFileSystem(infra::IFileSystem *fileSystem);
    void setRandomSource(infra::IRandomSource *randomSource);
    void setLogSink(infra::ILogSink *sink);
    String generateFortune();
    bool isLoaded();

private:
    std::vector<FortuneTemplate> templates;
    std::map<String, std::vector<String>> wordlists;
    bool loaded;

    infra::IFileSystem *m_fileSystem;
    infra::IRandomSource *m_randomSource;
    infra::ILogSink *m_logSink;

    String replaceTokens(const FortuneTemplate& fortuneTemplate, const std::map<String, String>& replacements);
    String getRandomWord(const String& category);
    bool validateTemplate(const FortuneTemplate& fortuneTemplate);
    void parseWordlists(JsonObject wordlistsObj);
    void parseTemplates(JsonArray templatesArray);
    std::vector<String> extractTokens(const String& templateText);
    infra::IFileSystem* resolveFileSystem();
    infra::IRandomSource* resolveRandomSource();
    infra::ILogSink* resolveLogSink();
    void log(infra::LogLevel level, const char *fmt, ...);
};

#endif // FORTUNE_GENERATOR_H
