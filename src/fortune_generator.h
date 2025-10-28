#ifndef FORTUNE_GENERATOR_H
#define FORTUNE_GENERATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

struct FortuneTemplate {
    String template_text;
    std::vector<String> tokens;
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
    
    String replaceTokens(const FortuneTemplate& fortuneTemplate, const std::map<String, String>& replacements);
    String getRandomWord(const String& category);
    bool validateTemplate(const FortuneTemplate& fortuneTemplate);
    void parseWordlists(JsonObject wordlistsObj);
    void parseTemplates(JsonArray templatesArray);
    std::vector<String> extractTokens(const String& templateText);
};

#endif // FORTUNE_GENERATOR_H
