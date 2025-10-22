#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <map>

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    bool loadConfig();
    String getBluetoothSpeakerName() const;
    String getRole() const;
    String getPrimaryMacAddress() const;
    String getSecondaryMacAddress() const;
    String getValue(const String& key, const String& defaultValue = "") const;
    int getSpeakerVolume() const { return speakerVolume; }
    void printConfig() const;
    int getServoMinDegrees() const;
    int getServoMaxDegrees() const;

private:
    ConfigManager() {}
    std::map<String, String> m_config;
    int speakerVolume;
    int m_servoMinDegrees;
    int m_servoMaxDegrees;

    void parseConfigLine(const String& line);
};

#endif // CONFIG_MANAGER_H