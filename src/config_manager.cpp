#include "config_manager.h"
#include "logging_manager.h"
#include "SD_MMC.h"

static constexpr const char* TAG = "ConfigManager";

ConfigManager &ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig()
{
    File configFile = SD_MMC.open("/config.txt", FILE_READ);
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
    // Default servo limits: conservative ±100 µs around neutral (1500 µs)
    // This provides a small, safe mouth opening that won't damage most servos
    // SD card config can override these if needed
    int servoMin = getValue("servo_us_min", "1400").toInt();
    int servoMax = getValue("servo_us_max", "1600").toInt();
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
    unsigned long fingerDetect = getValue("finger_detect_ms", "120").toInt();
    unsigned long fingerWait = getValue("finger_wait_ms", "6000").toInt();
    unsigned long snapDelayMin = getValue("snap_delay_min_ms", "1000").toInt();
    unsigned long snapDelayMax = getValue("snap_delay_max_ms", "3000").toInt();
    unsigned long cooldown = getValue("cooldown_ms", "12000").toInt();
    int mouthBright = getValue("mouth_led_bright", "255").toInt();
    int mouthPulseMin = getValue("mouth_led_pulse_min", "40").toInt();
    int mouthPulseMax = getValue("mouth_led_pulse_max", "255").toInt();
    unsigned long mouthPulsePeriod = getValue("mouth_led_pulse_period_ms", "1500").toInt();

    if (snapDelayMin >= snapDelayMax)
    {
        LOG_WARN(TAG, "Invalid snap delay timing (min >= max). Getters will return defaults.");
    }

    if (fingerDetect < 30 || fingerDetect > 1000)
    {
        LOG_WARN(TAG, "Finger detection debounce out of range (30-1000 ms expected). Getter will return default.");
    }

    if (fingerWait < 1000)
    {
        LOG_WARN(TAG, "Finger wait timeout too short (< 1000ms). Getters will return default.");
    }

    if (cooldown < 5000)
    {
        LOG_WARN(TAG, "Cooldown period too short (< 5000ms). Getters will return default.");
    }

    if (mouthBright < 0 || mouthBright > 255)
    {
        LOG_WARN(TAG, "Mouth LED bright value out of range (0-255). Getter will return default.");
    }

    if (mouthPulseMin < 0 || mouthPulseMin > 255 || mouthPulseMax < 0 || mouthPulseMax > 255 || mouthPulseMin > mouthPulseMax)
    {
        LOG_WARN(TAG, "Mouth LED pulse bounds invalid. Getters will fall back to defaults.");
    }

    if (mouthPulsePeriod < 200 || mouthPulsePeriod > 10000)
    {
        LOG_WARN(TAG, "Mouth LED pulse period out of range (200-10000 ms). Getter will return default.");
    }

    // Validate finger tuning parameters
    uint32_t fingerCyclesInit = strtoul(getValue("finger_cycles_init", "0x1000").c_str(), nullptr, 0);
    uint32_t fingerCyclesMeasure = strtoul(getValue("finger_cycles_measure", "0x1000").c_str(), nullptr, 0);
    if (fingerCyclesInit == 0 || fingerCyclesInit > 0xFFFF ||
        fingerCyclesMeasure == 0 || fingerCyclesMeasure > 0xFFFF)
    {
        LOG_WARN(TAG, "Finger touch cycles invalid (must be 1-0xFFFF). Getters will use defaults.");
    }

    float fingerAlpha = getValue("finger_filter_alpha", "0.3").toFloat();
    if (fingerAlpha < 0.0f || fingerAlpha > 1.0f)
    {
        LOG_WARN(TAG, "Finger filter alpha out of range (0.0-1.0). Getter will use default.");
    }

    float fingerDrift = getValue("finger_baseline_drift", "0.0001").toFloat();
    if (fingerDrift < 0.0f || fingerDrift > 0.1f)
    {
        LOG_WARN(TAG, "Finger baseline drift out of range (0-0.1). Getter will use default.");
    }

    int fingerMulti = getValue("finger_multisample", "32").toInt();
    if (fingerMulti <= 0 || fingerMulti > 255)
    {
        LOG_WARN(TAG, "Finger multisample count invalid (1-255). Getter will use default.");
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
    // Default: 1400 µs (narrow safe range until SD config expands it)
    int value = getValue("servo_us_min", "1400").toInt();
    int max = getServoUSMax();
    if (value >= max || value < 0) {
        return 1400; // Fallback to safe default
    }
    return value;
}

int ConfigManager::getServoUSMax() const
{
    // Default: 1600 µs (narrow safe range until SD config expands it)
    int value = getValue("servo_us_max", "1600").toInt();
    int min = getValue("servo_us_min", "1400").toInt(); // Get raw value
    if (value <= min || value > 5000) {
        return 1600; // Fallback to safe default
    }
    return value;
}

float ConfigManager::getCapThreshold() const
{
    float value = getValue("cap_threshold", "0.002").toFloat();
    if (value < 0.0001f || value > 1.0f) {
        return 0.002f; // Default
    }
    return value;
}

uint16_t ConfigManager::getFingerCyclesInit() const
{
    String val = getValue("finger_cycles_init", "0x1000");
    uint32_t parsed = strtoul(val.c_str(), nullptr, 0);
    if (parsed == 0 || parsed > 0xFFFF) {
        return 0x1000;
    }
    return static_cast<uint16_t>(parsed);
}

uint16_t ConfigManager::getFingerCyclesMeasure() const
{
    String val = getValue("finger_cycles_measure", "0x1000");
    uint32_t parsed = strtoul(val.c_str(), nullptr, 0);
    if (parsed == 0 || parsed > 0xFFFF) {
        return 0x1000;
    }
    return static_cast<uint16_t>(parsed);
}

float ConfigManager::getFingerFilterAlpha() const
{
    float value = getValue("finger_filter_alpha", "0.3").toFloat();
    if (value < 0.0f || value > 1.0f) {
        return 0.3f;
    }
    return value;
}

float ConfigManager::getFingerBaselineDrift() const
{
    float value = getValue("finger_baseline_drift", "0.0001").toFloat();
    if (value < 0.0f || value > 0.1f) {
        return 0.0001f;
    }
    return value;
}

uint8_t ConfigManager::getFingerMultisample() const
{
    int value = getValue("finger_multisample", "32").toInt();
    if (value <= 0 || value > 255) {
        return 32;
    }
    return static_cast<uint8_t>(value);
}

unsigned long ConfigManager::getFingerDetectMs() const
{
    unsigned long value = getValue("finger_detect_ms", "120").toInt();
    if (value < 30 || value > 1000) {
        return 120;
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
    return getValue("fortunes_json", "/fortunes/little_kid_fortunes.json");
}

uint8_t ConfigManager::getMouthLedBright() const
{
    int value = getValue("mouth_led_bright", "255").toInt();
    return static_cast<uint8_t>(constrain(value, 0, 255));
}

uint8_t ConfigManager::getMouthLedPulseMin() const
{
    int value = getValue("mouth_led_pulse_min", "40").toInt();
    return static_cast<uint8_t>(constrain(value, 0, 255));
}

uint8_t ConfigManager::getMouthLedPulseMax() const
{
    int value = getValue("mouth_led_pulse_max", "255").toInt();
    return static_cast<uint8_t>(constrain(value, 0, 255));
}

unsigned long ConfigManager::getMouthLedPulsePeriodMs() const
{
    unsigned long value = getValue("mouth_led_pulse_period_ms", "1500").toInt();
    if (value < 200) {
        return 1500;
    }
    return value;
}
