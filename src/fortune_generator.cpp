#include "fortune_generator.h"
#include "infra/filesystem.h"
#include "infra/random_source.h"
#include <ArduinoJson.h>
#include <cstdarg>
#include <cstdio>
#ifndef UNIT_TEST
#include "infra/sd_mmc_filesystem.h"
#include "infra/arduino_random_source.h"
#include "logging_manager.h"
#include "SD_MMC.h"
#endif
#include "infra/log_sink.h"

static constexpr const char* TAG = "FortuneGenerator";

FortuneGenerator::FortuneGenerator()
    : loaded(false),
      m_fileSystem(nullptr),
      m_randomSource(nullptr),
      m_logSink(nullptr) {
}

void FortuneGenerator::setFileSystem(infra::IFileSystem *fileSystem) {
    m_fileSystem = fileSystem;
}

void FortuneGenerator::setRandomSource(infra::IRandomSource *randomSource) {
    m_randomSource = randomSource;
}

void FortuneGenerator::setLogSink(infra::ILogSink *sink) {
    m_logSink = sink;
    infra::setLogSink(sink);
}

bool FortuneGenerator::loadFortunes(const String& filePath) {
    infra::IFileSystem *fs = resolveFileSystem();
    resolveLogSink();
    if (!fs) {
        log(infra::LogLevel::Error, "No filesystem available for fortune loading");
        return false;
    }

    auto file = fs->open(filePath.c_str(), FILE_READ);
    if (!file) {
        log(infra::LogLevel::Error, "Failed to open fortune file: %s", filePath.c_str());
        return false;
    }

    String jsonString = file->readString();
    file->close();
    
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    DynamicJsonDocument doc(2048);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    DeserializationError error = deserializeJson(doc, jsonString.c_str());

    if (error) {
        log(infra::LogLevel::Error, "Failed to parse fortune JSON: %s", error.c_str());
        return false;
    }

    // Parse version
    if (!doc["version"].is<int>()) {
        log(infra::LogLevel::Error, "Fortune file missing version");
        return false;
    }

    // Parse templates
    if (!doc["templates"].is<JsonArray>()) {
        log(infra::LogLevel::Error, "Fortune file missing or invalid templates");
        return false;
    }
    parseTemplates(doc["templates"]);

    // Parse wordlists
    if (!doc["wordlists"].is<JsonObject>()) {
        log(infra::LogLevel::Error, "Fortune file missing or invalid wordlists");
        return false;
    }
    parseWordlists(doc["wordlists"]);
    
    // Validate all templates
    for (const auto& template_obj : templates) {
        if (!validateTemplate(template_obj)) {
            log(infra::LogLevel::Error, "Invalid template: %s", template_obj.template_text.c_str());
            return false;
        }
    }

    loaded = true;
    log(infra::LogLevel::Info, "Loaded %u fortune templates", static_cast<unsigned>(templates.size()));
    return true;
}

String FortuneGenerator::generateFortune() {
    resolveLogSink();
    if (!loaded || templates.empty()) {
        log(infra::LogLevel::Warn, "generateFortune called before templates loaded");
        return "The spirits are silent...";
    }

    infra::IRandomSource *randomSource = resolveRandomSource();
    if (!randomSource) {
        log(infra::LogLevel::Error, "Random source unavailable; returning fallback fortune");
        return "The spirits are silent...";
    }

    // Select random template
    int templateIndex = randomSource->nextInt(0, static_cast<int>(templates.size()));
    const FortuneTemplate &fortuneTemplate = templates[templateIndex];

    if (fortuneTemplate.tokens.empty()) {
        log(infra::LogLevel::Warn, "Template has no tokens; returning literal text");
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
            log(infra::LogLevel::Warn, "Unterminated token in template: %s", templateText.c_str());
            result += templateText.substring(tokenStart);
            break;
        }

        String token = templateText.substring(tokenStart + 2, tokenEnd);
        token.trim();
        auto it = replacements.find(token);
        if (it != replacements.end()) {
            result += it->second;
        } else {
            log(infra::LogLevel::Warn, "Missing replacement for token '%s'; leaving placeholder", token.c_str());
            result += "{{" + token + "}}";
        }
        index = tokenEnd + 2;
    }

    return result;
}

String FortuneGenerator::getRandomWord(const String& category) {
    resolveLogSink();
    if (wordlists.find(category) != wordlists.end() && !wordlists[category].empty()) {
        infra::IRandomSource *randomSource = resolveRandomSource();
        if (!randomSource) {
            log(infra::LogLevel::Error, "Random source unavailable when fetching token '%s'", category.c_str());
            return wordlists[category].front();
        }
        int index = randomSource->nextInt(0, static_cast<int>(wordlists[category].size()));
        return wordlists[category][index];
    }
    log(infra::LogLevel::Warn, "Wordlist missing or empty for token '%s'", category.c_str());
    return "mystery"; // Fallback word
}

bool FortuneGenerator::validateTemplate(const FortuneTemplate& fortuneTemplate) {
    if (fortuneTemplate.tokens.empty()) {
        log(infra::LogLevel::Warn, "Template has no tokens: %s", fortuneTemplate.template_text.c_str());
    }

    for (const auto &token : fortuneTemplate.tokens) {
        if (wordlists.find(token) == wordlists.end() || wordlists[token].empty()) {
            log(infra::LogLevel::Warn, "Token '%s' has no wordlist or empty wordlist", token.c_str());
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
            const char *value = word.as<const char*>();
            if (value) {
                wordList.emplace_back(value);
            }
        }

        wordlists[category] = wordList;
        log(infra::LogLevel::Info, "Loaded %u words for category '%s'", static_cast<unsigned>(wordList.size()), category.c_str());
    }
}

void FortuneGenerator::parseTemplates(JsonArray templatesArray) {
    templates.clear();
    
    for (JsonVariant template_var : templatesArray) {
        FortuneTemplate template_obj;
        const char *text = template_var.as<const char*>();
        if (text) {
            template_obj.template_text = text;
            template_obj.tokens = extractTokens(template_obj.template_text);
            templates.push_back(template_obj);
        }
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

infra::IFileSystem* FortuneGenerator::resolveFileSystem() {
    if (m_fileSystem) {
        return m_fileSystem;
    }
#ifndef UNIT_TEST
    static infra::SDMMCFileSystem defaultFs;
    return &defaultFs;
#else
    return nullptr;
#endif
}

infra::IRandomSource* FortuneGenerator::resolveRandomSource() {
    if (m_randomSource) {
        return m_randomSource;
    }
#ifndef UNIT_TEST
    static infra::ArduinoRandomSource defaultRandom;
    m_randomSource = &defaultRandom;
    return m_randomSource;
#else
    return nullptr;
#endif
}

infra::ILogSink* FortuneGenerator::resolveLogSink() {
    if (m_logSink) {
        return m_logSink;
    }
    return infra::getLogSink();
}

void FortuneGenerator::log(infra::LogLevel level, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (infra::ILogSink *sink = resolveLogSink()) {
        sink->log(level, TAG, buffer);
    }
#ifndef UNIT_TEST
    else {
        LoggingManager::instance().log(static_cast<::LogLevel>(level), TAG, "%s", buffer);
    }
#else
    (void)level;
    (void)buffer;
#endif
}
