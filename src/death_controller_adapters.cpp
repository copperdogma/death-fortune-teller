#include "death_controller_adapters.h"

#ifdef ARDUINO

#include "audio_directory_selector.h"
#include "audio_player.h"
#include "fortune_generator.h"
#include "thermal_printer.h"
#include "light_controller.h"
#include "finger_sensor.h"
#include "config_manager.h"

#include "infra/log_sink.h"

namespace {
constexpr const char *kTag = "DeathControllerAdapters";
}

AudioPlannerAdapter::AudioPlannerAdapter(AudioDirectorySelector &selector)
    : m_selector(selector), m_audioPlayer(nullptr) {
}

void AudioPlannerAdapter::setAudioPlayer(AudioPlayer *player) {
    m_audioPlayer = player;
}

bool AudioPlannerAdapter::hasAvailableClip(const std::string &directory, const char *label) {
    auto cached = m_cachedSelections.find(directory);
    if (cached != m_cachedSelections.end()) {
        return !cached->second.empty();
    }

    String clip = m_selector.selectClip(directory.c_str(), label);
    if (clip.isEmpty()) {
        m_cachedSelections[directory] = "";
        return false;
    }

    m_cachedSelections[directory] = std::string(clip.c_str());
    return true;
}

std::string AudioPlannerAdapter::pickClip(const std::string &directory, const char *label) {
    auto cached = m_cachedSelections.find(directory);
    if (cached != m_cachedSelections.end()) {
        std::string clip = cached->second;
        m_cachedSelections.erase(cached);
        return clip;
    }

    String clip = m_selector.selectClip(directory.c_str(), label);
    if (clip.isEmpty()) {
        return {};
    }
    return std::string(clip.c_str());
}

bool AudioPlannerAdapter::isAudioPlaying() const {
    return m_audioPlayer ? m_audioPlayer->isAudioPlaying() : false;
}

FortuneServiceAdapter::FortuneServiceAdapter(FortuneGenerator &generator)
    : m_generator(generator) {
}

bool FortuneServiceAdapter::ensureLoaded(const std::string &path) {
    if (path.empty()) {
        return m_generator.isLoaded();
    }

    if (m_generator.isLoaded() && path == m_loadedPath) {
        return true;
    }

    bool loaded = m_generator.loadFortunes(String(path.c_str()));
    if (loaded) {
        m_loadedPath = path;
    } else {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Failed to load fortunes from %s", path.c_str());
    }
    return loaded;
}

std::string FortuneServiceAdapter::generateFortune() {
    String fortune = m_generator.generateFortune();
    return std::string(fortune.c_str());
}

PrinterStatusAdapter::PrinterStatusAdapter(ThermalPrinter &printer)
    : m_printer(printer) {
}

bool PrinterStatusAdapter::isReady() const {
    return m_printer.isReady();
}

ManualCalibrationAdapter::ManualCalibrationAdapter(LightController &lights, FingerSensor &sensor, ConfigManager &config)
    : m_lights(lights), m_sensor(sensor), m_config(config) {
}

void ManualCalibrationAdapter::startPreBlink() {
    uint8_t brightness = static_cast<uint8_t>(m_config.getMouthLedBright());
    m_lights.startMouthBlinkSequence(3, 120, 120, brightness, false, "Manual calibration start");
}

void ManualCalibrationAdapter::setWaitMode() {
    m_lights.setMouthBright();
}

void ManualCalibrationAdapter::calibrateSensor() {
    m_sensor.calibrate();
}

void ManualCalibrationAdapter::startCompletionBlink() {
    uint8_t brightness = static_cast<uint8_t>(m_config.getMouthLedBright());
    m_lights.startMouthBlinkSequence(4, 120, 120, brightness, false, "Manual calibration finished");
}

bool ManualCalibrationAdapter::isBlinking() const {
    return m_lights.isMouthBlinking();
}

#endif  // ARDUINO