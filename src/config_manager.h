#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <map>

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    bool loadConfig();
    String getBluetoothSpeakerName() const;
    String getRole() const;
    String getValue(const String& key, const String& defaultValue = "") const;
    int getSpeakerVolume() const { return speakerVolume; }
    void printConfig() const;
    int getServoMinDegrees() const;
    int getServoMaxDegrees() const;
    
    // WiFi configuration methods
    String getWiFiSSID() const;
    String getWiFiPassword() const;
    String getOTAHostname() const;
    String getOTAPassword() const;

    bool isBluetoothEnabled() const;

    // Servo configuration (microseconds)
    int getServoUSMin() const;
    int getServoUSMax() const;
    bool getServoReverse() const;

    // Capacitive sensor configuration
    float getCapThreshold() const;
    uint16_t getFingerCyclesInit() const;
    uint16_t getFingerCyclesMeasure() const;
    float getFingerFilterAlpha() const;
    float getFingerBaselineDrift() const;
    uint8_t getFingerMultisample() const;

    // Timing configuration
    unsigned long getFingerDetectMs() const;
    unsigned long getFingerWaitMs() const;
    unsigned long getSnapDelayMinMs() const;
    unsigned long getSnapDelayMaxMs() const;
    unsigned long getCooldownMs() const;

    // Printer configuration
    int getPrinterBaud() const;
    String getPrinterLogo() const;
    String getFortunesJson() const;

    // Mouth LED configuration
    uint8_t getMouthLedBright() const;
    uint8_t getMouthLedPulseMin() const;
    uint8_t getMouthLedPulseMax() const;
    unsigned long getMouthLedPulsePeriodMs() const;

private:
    ConfigManager() {}
    std::map<String, String> m_config;
    int speakerVolume;
    int m_servoMinDegrees;
    int m_servoMaxDegrees;

    void parseConfigLine(const String& line);
};

#endif // CONFIG_MANAGER_H
