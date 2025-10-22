#ifndef FORTUNE_GENERATOR_H
#define FORTUNE_GENERATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

struct FortuneTemplate {
    String template_text;
    std::vector<String> wordlists;
};

class FortuneGenerator {
public:
    FortuneGenerator();
    bool loadFortunes(const String& filePath);
    String generateFortune();
    bool isLoaded();

private:
    std::vector<FortuneTemplate> templates;
    std::map<String, std::vector<String>> wordlists;
    bool loaded;
    
    String replaceTokens(const String& template_text, const std::map<String, String>& replacements);
    String getRandomWord(const String& category);
    bool validateTemplate(const String& template_text);
    void parseWordlists(JsonObject wordlistsObj);
    void parseTemplates(JsonArray templatesArray);
};

#endif // FORTUNE_GENERATOR_H
