#include "fortune_generator.h"
#include <SD.h>
#include <ArduinoJson.h>

FortuneGenerator::FortuneGenerator() : loaded(false) {
}

bool FortuneGenerator::loadFortunes(const String& filePath) {
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        Serial.printf("Failed to open fortune file: %s\n", filePath.c_str());
        return false;
    }
    
    String jsonString = file.readString();
    file.close();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.printf("Failed to parse fortune JSON: %s\n", error.c_str());
        return false;
    }
    
    // Parse version
    if (!doc.containsKey("version")) {
        Serial.println("Fortune file missing version");
        return false;
    }
    
    // Parse templates
    if (!doc.containsKey("templates") || !doc["templates"].is<JsonArray>()) {
        Serial.println("Fortune file missing or invalid templates");
        return false;
    }
    parseTemplates(doc["templates"]);
    
    // Parse wordlists
    if (!doc.containsKey("wordlists") || !doc["wordlists"].is<JsonObject>()) {
        Serial.println("Fortune file missing or invalid wordlists");
        return false;
    }
    parseWordlists(doc["wordlists"]);
    
    // Validate all templates
    for (const auto& template_obj : templates) {
        if (!validateTemplate(template_obj.template_text)) {
            Serial.printf("Invalid template: %s\n", template_obj.template_text.c_str());
            return false;
        }
    }
    
    loaded = true;
    Serial.printf("Loaded %d fortune templates\n", templates.size());
    return true;
}

String FortuneGenerator::generateFortune() {
    if (!loaded || templates.empty()) {
        return "The spirits are silent...";
    }
    
    // Select random template
    int templateIndex = random(0, templates.size());
    String template_text = templates[templateIndex].template_text;
    
    // Find all tokens in the template
    String result = template_text;
    int startPos = 0;
    
    while (true) {
        int tokenStart = result.indexOf("{{", startPos);
        if (tokenStart == -1) break;
        
        int tokenEnd = result.indexOf("}}", tokenStart);
        if (tokenEnd == -1) break;
        
        String token = result.substring(tokenStart + 2, tokenEnd);
        String replacement = getRandomWord(token);
        
        result.replace("{{" + token + "}}", replacement);
        startPos = tokenStart + replacement.length();
    }
    
    return result;
}

bool FortuneGenerator::isLoaded() {
    return loaded;
}

String FortuneGenerator::replaceTokens(const String& template_text, const std::map<String, String>& replacements) {
    String result = template_text;
    
    for (const auto& pair : replacements) {
        String token = "{{" + pair.first + "}}";
        result.replace(token, pair.second);
    }
    
    return result;
}

String FortuneGenerator::getRandomWord(const String& category) {
    if (wordlists.find(category) != wordlists.end() && !wordlists[category].empty()) {
        int index = random(0, wordlists[category].size());
        return wordlists[category][index];
    }
    
    return "mystery"; // Fallback word
}

bool FortuneGenerator::validateTemplate(const String& template_text) {
    // Check that all tokens have corresponding wordlists
    int startPos = 0;
    
    while (true) {
        int tokenStart = template_text.indexOf("{{", startPos);
        if (tokenStart == -1) break;
        
        int tokenEnd = template_text.indexOf("}}", tokenStart);
        if (tokenEnd == -1) return false;
        
        String token = template_text.substring(tokenStart + 2, tokenEnd);
        
        if (wordlists.find(token) == wordlists.end() || wordlists[token].empty()) {
            Serial.printf("Token '%s' has no wordlist or empty wordlist\n", token.c_str());
            return false;
        }
        
        startPos = tokenEnd + 2;
    }
    
    return true;
}

void FortuneGenerator::parseWordlists(JsonObject wordlistsObj) {
    wordlists.clear();
    
    for (JsonPair pair : wordlistsObj) {
        String category = pair.key().c_str();
        JsonArray words = pair.value().as<JsonArray>();
        
        std::vector<String> wordList;
        for (JsonVariant word : words) {
            wordList.push_back(word.as<String>());
        }
        
        wordlists[category] = wordList;
        Serial.printf("Loaded %d words for category '%s'\n", wordList.size(), category.c_str());
    }
}

void FortuneGenerator::parseTemplates(JsonArray templatesArray) {
    templates.clear();
    
    for (JsonVariant template_var : templatesArray) {
        FortuneTemplate template_obj;
        template_obj.template_text = template_var.as<String>();
        templates.push_back(template_obj);
    }
}
