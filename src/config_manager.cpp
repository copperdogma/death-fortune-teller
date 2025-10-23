#include "config_manager.h"
#include "logging_manager.h"

static constexpr const char* TAG = "ConfigManager";

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
        LOG_ERROR(TAG, "Failed to open config file");
        return false;
    }

    LOG_INFO(TAG, "📄 Reading configuration file:");
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

                String loggedValue = value;
                if (key.equalsIgnoreCase("wifi_password") || key.equalsIgnoreCase("ota_password"))
                {
                    loggedValue = value.length() > 0 ? "[SET]" : "[NOT SET]";
                }

                LOG_DEBUG(TAG, "  %s: %s", key.c_str(), loggedValue.c_str());
            }
        }
    }

    configFile.close();

    // Validate speaker volume
    speakerVolume = getValue("speaker_volume", "100").toInt();
    if (speakerVolume < 0 || speakerVolume > 100)
    {
        LOG_WARN(TAG, "Invalid speaker volume. Using default value of 100.");
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
        LOG_INFO(TAG, "%s: %s", pair.first.c_str(), pair.second.c_str());
    }
    LOG_INFO(TAG, "Speaker Volume: %d", speakerVolume);
}

String ConfigManager::getWiFiSSID() const
{
    return getValue("wifi_ssid", "");
}

String ConfigManager::getWiFiPassword() const
{
    return getValue("wifi_password", "");
}

String ConfigManager::getOTAHostname() const
{
    return getValue("ota_hostname", "death-fortune-teller");
}

String ConfigManager::getOTAPassword() const
{
    return getValue("ota_password", "");
}

bool ConfigManager::isRemoteDebugEnabled() const
{
    return getValue("remote_debug_enabled", "false").equalsIgnoreCase("true");
}

int ConfigManager::getRemoteDebugPort() const
{
    return getValue("remote_debug_port", "23").toInt();
}
