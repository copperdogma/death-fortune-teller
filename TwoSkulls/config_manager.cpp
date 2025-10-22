#include "config_manager.h"

ConfigManager &ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig()
{
    File configFile = SD.open("/config.txt", FILE_READ);
    if (!configFile)
    {
        Serial.println("Failed to open config file");
        return false;
    }

    Serial.println("Reading configuration file:");
    while (configFile.available())
    {
        String line = configFile.readStringUntil('\n');
        line.trim();
        if (line.length() > 0 && line[0] != '#')
        {
            parseConfigLine(line);
            // Output the key-value pair
            int separatorIndex = line.indexOf('=');
            if (separatorIndex != -1)
            {
                String key = line.substring(0, separatorIndex);
                String value = line.substring(separatorIndex + 1);
                key.trim();
                value.trim();
                Serial.printf("  %s: %s\n", key.c_str(), value.c_str());
            }
        }
    }

    configFile.close();

    // Validate speaker volume
    speakerVolume = getValue("speaker_volume", "100").toInt();
    if (speakerVolume < 0 || speakerVolume > 100)
    {
        Serial.println("Invalid speaker volume. Using default value of 100.");
        speakerVolume = 100;
    }

    // Hardcode servo min/max degrees for now
    m_servoMinDegrees = 0;  // Default min degrees
    m_servoMaxDegrees = 80; // Default max degrees

    return true;
}

void ConfigManager::parseConfigLine(const String &line)
{
    int separatorIndex = line.indexOf('=');
    if (separatorIndex != -1)
    {
        String key = line.substring(0, separatorIndex);
        String value = line.substring(separatorIndex + 1);
        key.trim();
        value.trim();
        m_config[key] = value;
    }
}

String ConfigManager::getValue(const String &key, const String &defaultValue) const
{
    auto it = m_config.find(key);
    return (it != m_config.end()) ? it->second : defaultValue;
}

String ConfigManager::getBluetoothSpeakerName() const
{
    return getValue("speaker_name", "Unknown Speaker");
}

String ConfigManager::getRole() const
{
    return getValue("role", "unknown");
}

String ConfigManager::getPrimaryMacAddress() const
{
    return getValue("primary_mac_address", "unknown");
}

String ConfigManager::getSecondaryMacAddress() const
{
    return getValue("secondary_mac_address", "unknown");
}

int ConfigManager::getServoMinDegrees() const
{
    return m_servoMinDegrees;
}

int ConfigManager::getServoMaxDegrees() const
{
    return m_servoMaxDegrees;
}

void ConfigManager::printConfig() const
{
    for (const auto &pair : m_config)
    {
        Serial.printf("%s: %s\n", pair.first.c_str(), pair.second.c_str());
    }
    Serial.printf("Speaker Volume: %d\n", speakerVolume);
}