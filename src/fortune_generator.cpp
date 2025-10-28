#include "fortune_generator.h"
#include "logging_manager.h"
#include "SD_MMC.h"
#include <ArduinoJson.h>

static constexpr const char* TAG = "FortuneGenerator";

FortuneGenerator::FortuneGenerator() : loaded(false) {
}

bool FortuneGenerator::loadFortunes(const String& filePath) {
    File file = SD_MMC.open(filePath, FILE_READ);
    if (!file) {
        LOG_ERROR(TAG, "Failed to open fortune file: %s", filePath.c_str());
        return false;
    }
    
    String jsonString = file.readString();
    file.close();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        LOG_ERROR(TAG, "Failed to parse fortune JSON: %s", error.c_str());
        return false;
    }
    
    // Parse version
    if (!doc.containsKey("version")) {
        LOG_ERROR(TAG, "Fortune file missing version");
        return false;
    }
    
    // Parse templates
    if (!doc.containsKey("templates") || !doc["templates"].is<JsonArray>()) {
        LOG_ERROR(TAG, "Fortune file missing or invalid templates");
        return false;
    }
    parseTemplates(doc["templates"]);
    
    // Parse wordlists
    if (!doc.containsKey("wordlists") || !doc["wordlists"].is<JsonObject>()) {
        LOG_ERROR(TAG, "Fortune file missing or invalid wordlists");
        return false;
    }
    parseWordlists(doc["wordlists"]);
    
    // Validate all templates
    for (const auto& template_obj : templates) {
        if (!validateTemplate(template_obj)) {
            LOG_ERROR(TAG, "Invalid template: %s", template_obj.template_text.c_str());
            return false;
        }
    }

    loaded = true;
    LOG_INFO(TAG, "Loaded %d fortune templates", templates.size());
    return true;
}

String FortuneGenerator::generateFortune() {
    if (!loaded || templates.empty()) {
        LOG_WARN(TAG, "generateFortune called before templates loaded");
        return "The spirits are silent...";
    }
    
    // Select random template
    int templateIndex = random(0, templates.size());
    const FortuneTemplate &fortuneTemplate = templates[templateIndex];

    if (fortuneTemplate.tokens.empty()) {
        LOG_WARN(TAG, "Template has no tokens; returning literal text");
        return fortuneTemplate.template_text;
    }

    std::map<String, String> replacements;
    for (const auto &token : fortuneTemplate.tokens) {
        String replacement = getRandomWord(token);
        replacements[token] = replacement;
    }

    return replaceTokens(fortuneTemplate, replacements);
}

bool FortuneGenerator::isLoaded() {
    return loaded;
}

String FortuneGenerator::replaceTokens(const FortuneTemplate& fortuneTemplate, const std::map<String, String>& replacements) {
    String result;
    const String &templateText = fortuneTemplate.template_text;

    int length = templateText.length();
    int index = 0;
    while (index < length) {
        int tokenStart = templateText.indexOf("{{", index);
        if (tokenStart == -1) {
            result += templateText.substring(index);
            break;
        }

        result += templateText.substring(index, tokenStart);
        int tokenEnd = templateText.indexOf("}}", tokenStart + 2);
        if (tokenEnd == -1) {
            LOG_WARN(TAG, "Unterminated token in template: %s", templateText.c_str());
            result += templateText.substring(tokenStart);
            break;
        }

        String token = templateText.substring(tokenStart + 2, tokenEnd);
        token.trim();
        auto it = replacements.find(token);
        if (it != replacements.end()) {
            result += it->second;
        } else {
            LOG_WARN(TAG, "Missing replacement for token '%s'; leaving placeholder", token.c_str());
            result += "{{" + token + "}}";
        }
        index = tokenEnd + 2;
    }

    return result;
}

String FortuneGenerator::getRandomWord(const String& category) {
    if (wordlists.find(category) != wordlists.end() && !wordlists[category].empty()) {
        int index = random(0, wordlists[category].size());
        return wordlists[category][index];
    }
    LOG_WARN(TAG, "Wordlist missing or empty for token '%s'", category.c_str());
    return "mystery"; // Fallback word
}

bool FortuneGenerator::validateTemplate(const FortuneTemplate& fortuneTemplate) {
    if (fortuneTemplate.tokens.empty()) {
        LOG_WARN(TAG, "Template has no tokens: %s", fortuneTemplate.template_text.c_str());
    }

    for (const auto &token : fortuneTemplate.tokens) {
        if (wordlists.find(token) == wordlists.end() || wordlists[token].empty()) {
            LOG_WARN(TAG, "Token '%s' has no wordlist or empty wordlist", token.c_str());
            return false;
        }
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
        LOG_INFO(TAG, "Loaded %d words for category '%s'", wordList.size(), category.c_str());
    }
}

void FortuneGenerator::parseTemplates(JsonArray templatesArray) {
    templates.clear();
    
    for (JsonVariant template_var : templatesArray) {
        FortuneTemplate template_obj;
        template_obj.template_text = template_var.as<String>();
        template_obj.tokens = extractTokens(template_obj.template_text);
        templates.push_back(template_obj);
    }
}

std::vector<String> FortuneGenerator::extractTokens(const String& templateText) {
    std::vector<String> tokens;
    int startPos = 0;
    while (true) {
        int tokenStart = templateText.indexOf("{{", startPos);
        if (tokenStart == -1) break;
        int tokenEnd = templateText.indexOf("}}", tokenStart + 2);
        if (tokenEnd == -1) break;
        String token = templateText.substring(tokenStart + 2, tokenEnd);
        token.trim();
        if (token.length() > 0) {
            bool exists = false;
            for (const auto &existing : tokens) {
                if (existing == token) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                tokens.push_back(token);
            }
        }
        startPos = tokenEnd + 2;
    }
    return tokens;
}
