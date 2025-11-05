#ifndef DEATH_CONTROLLER_ADAPTERS_H
#define DEATH_CONTROLLER_ADAPTERS_H

#include <map>
#include <string>

#include "death_controller.h"

class LightController;
class FingerSensor;
class ConfigManager;

class AudioPlayer;
class AudioDirectorySelector;
class FortuneGenerator;
class ThermalPrinter;

class AudioPlannerAdapter : public DeathController::IAudioPlanner {
public:
    explicit AudioPlannerAdapter(AudioDirectorySelector &selector);

    void setAudioPlayer(AudioPlayer *player);

    bool hasAvailableClip(const std::string &directory, const char *label = nullptr) override;
    std::string pickClip(const std::string &directory, const char *label = nullptr) override;
    bool isAudioPlaying() const override;

private:
    AudioDirectorySelector &m_selector;
    AudioPlayer *m_audioPlayer;
    std::map<std::string, std::string> m_cachedSelections;
};

class FortuneServiceAdapter : public DeathController::IFortuneService {
public:
    explicit FortuneServiceAdapter(FortuneGenerator &generator);

    bool ensureLoaded(const std::string &path) override;
    std::string generateFortune() override;

private:
    FortuneGenerator &m_generator;
    std::string m_loadedPath;
};

class PrinterStatusAdapter : public DeathController::IPrinterStatus {
public:
    explicit PrinterStatusAdapter(ThermalPrinter &printer);
    bool isReady() const override;

private:
    ThermalPrinter &m_printer;
};

class ManualCalibrationAdapter : public DeathController::IManualCalibrationDriver {
public:
    ManualCalibrationAdapter(LightController &lights, FingerSensor &sensor, ConfigManager &config);

    void startPreBlink() override;
    void setWaitMode() override;
    void calibrateSensor() override;
    void startCompletionBlink() override;
    bool isBlinking() const override;

private:
    LightController &m_lights;
    FingerSensor &m_sensor;
    ConfigManager &m_config;
};

#endif  // DEATH_CONTROLLER_ADAPTERS_H
