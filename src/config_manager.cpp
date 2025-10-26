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

    // Note: Validation happens in getter methods - they return default values if invalid
    // We just validate here to log warnings
    int servoMin = getValue("servo_us_min", "500").toInt();
    int servoMax = getValue("servo_us_max", "2500").toInt();
    if (servoMin >= servoMax)
    {
        LOG_WARN(TAG, "Invalid servo timing (min >= max). Getters will return defaults.");
    }

    // Validate capacitive threshold
    float capThreshold = getValue("cap_threshold", "0.002").toFloat();
    if (capThreshold < 0.001f || capThreshold > 0.1f)
    {
        LOG_WARN(TAG, "Invalid cap threshold (0.001-0.1 expected). Getters will return default.");
    }

    // Validate timing values
    unsigned long fingerWait = getValue("finger_wait_ms", "6000").toInt();
    unsigned long snapDelayMin = getValue("snap_delay_min_ms", "1000").toInt();
    unsigned long snapDelayMax = getValue("snap_delay_max_ms", "3000").toInt();
    unsigned long cooldown = getValue("cooldown_ms", "12000").toInt();

    if (snapDelayMin >= snapDelayMax)
    {
        LOG_WARN(TAG, "Invalid snap delay timing (min >= max). Getters will return defaults.");
    }

    if (fingerWait < 1000)
    {
        LOG_WARN(TAG, "Finger wait timeout too short (< 1000ms). Getters will return default.");
    }

    if (cooldown < 5000)
    {
        LOG_WARN(TAG, "Cooldown period too short (< 5000ms). Getters will return default.");
    }

    // Validate printer baud rate
    int printerBaud = getValue("printer_baud", "9600").toInt();
    if (printerBaud < 1200 || printerBaud > 115200)
    {
        LOG_WARN(TAG, "Invalid printer baud rate. Getters will return default of 9600.");
    }

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

bool ConfigManager::isBluetoothEnabled() const
{
    return !getValue("bluetooth_enabled", "true").equalsIgnoreCase("false");
}

int ConfigManager::getServoUSMin() const
{
    int value = getValue("servo_us_min", "500").toInt();
    int max = getServoUSMax();
    if (value >= max || value < 0) {
        return 500; // Default
    }
    return value;
}

int ConfigManager::getServoUSMax() const
{
    int value = getValue("servo_us_max", "2500").toInt();
    int min = getValue("servo_us_min", "500").toInt(); // Get raw value
    if (value <= min || value > 5000) {
        return 2500; // Default
    }
    return value;
}

float ConfigManager::getCapThreshold() const
{
    float value = getValue("cap_threshold", "0.002").toFloat();
    if (value < 0.001f || value > 0.1f) {
        return 0.002f; // Default
    }
    return value;
}

unsigned long ConfigManager::getFingerWaitMs() const
{
    unsigned long value = getValue("finger_wait_ms", "6000").toInt();
    if (value < 1000) {
        return 6000; // Default
    }
    return value;
}

unsigned long ConfigManager::getSnapDelayMinMs() const
{
    unsigned long value = getValue("snap_delay_min_ms", "1000").toInt();
    unsigned long max = getValue("snap_delay_max_ms", "3000").toInt(); // Get raw value
    if (value >= max || value < 100) {
        return 1000; // Default
    }
    return value;
}

unsigned long ConfigManager::getSnapDelayMaxMs() const
{
    unsigned long value = getValue("snap_delay_max_ms", "3000").toInt();
    unsigned long min = getValue("snap_delay_min_ms", "1000").toInt(); // Get raw value
    if (value <= min || value > 10000) {
        return 3000; // Default
    }
    return value;
}

unsigned long ConfigManager::getCooldownMs() const
{
    unsigned long value = getValue("cooldown_ms", "12000").toInt();
    if (value < 5000) {
        return 12000; // Default
    }
    return value;
}

int ConfigManager::getPrinterBaud() const
{
    int value = getValue("printer_baud", "9600").toInt();
    if (value < 1200 || value > 115200) {
        return 9600; // Default
    }
    return value;
}

String ConfigManager::getPrinterLogo() const
{
    return getValue("printer_logo", "/printer/logo_384w.bmp");
}

String ConfigManager::getFortunesJson() const
{
    return getValue("fortunes_json", "/printer/fortunes_littlekid.json");
}
