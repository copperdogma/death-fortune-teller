/*
  Death, the Fortune-Telling Skeleton
  Main file for controlling an animatronic skull that reads fortunes.
  Based on TwoSkulls but adapted for single skull with Matter control.
*/

#include <Arduino.h>
#include "audio_player.h"
#include "bluetooth_controller.h"
#include "servo_controller.h"
#include "sd_card_manager.h"
#include "skull_audio_animator.h"
#include "config_manager.h"
#include "uart_controller.h"
#include "thermal_printer.h"
#include "finger_sensor.h"
#include "fortune_generator.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "remote_debug_manager.h"
#include "logging_manager.h"
#include "death_controller.h"
#include "death_controller_adapters.h"
#include "infra/log_sink.h"
#include "infra/arduino_time_provider.h"
#include "infra/arduino_random_source.h"
#include "skit_selector.h"
#include "audio_directory_selector.h"
#include <WiFi.h>
#include "SD_MMC.h"
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "esp_attr.h"
#include <vector>
#include <algorithm>
#include <string>

// Pin definitions
const int EYE_LED_PIN = 32;    // GPIO pin for eye LED
const int MOUTH_LED_PIN = 33;  // GPIO pin for mouth LED
const int SERVO_PIN = 23;      // Servo control pin (moved off SD CMD line)
const int CAP_SENSE_PIN = 4;   // Capacitive finger sensor pin
const int PRINTER_TX_PIN = 18; // Thermal printer TX pin (ESP32 -> printer RXD)
const int PRINTER_RX_PIN = 19; // Thermal printer RX pin (printer TXD -> ESP32)
const int MATTER_TX_PIN = 21;  // Matter UART TX pin (ESP32 (green)-> ESP32-C3 RX GPIO06)
const int MATTER_RX_PIN = 22;  // Matter UART RX pin (ESP32 (yellow) <- ESP32-C3 TX GPIO05)

// Global objects
LightController lightController(EYE_LED_PIN, MOUTH_LED_PIN);
ServoController servoController;
SDCardManager *sdCardManager = nullptr;
AudioPlayer *audioPlayer = nullptr;
BluetoothController *bluetoothController = nullptr;
UARTController *uartController = nullptr;
ThermalPrinter *thermalPrinter = nullptr;
FingerSensor *fingerSensor = nullptr;
FortuneGenerator *fortuneGenerator = nullptr;
SkullAudioAnimator *skullAudioAnimator = nullptr;
WiFiManager *wifiManager = nullptr;
OTAManager *otaManager = nullptr;
RemoteDebugManager *remoteDebugManager = nullptr;
SkitSelector *skitSelector = nullptr;
AudioDirectorySelector *audioDirectorySelector = nullptr;
AudioPlannerAdapter *audioPlannerAdapter = nullptr;
FortuneServiceAdapter *fortuneServiceAdapter = nullptr;
PrinterStatusAdapter *printerStatusAdapter = nullptr;
ManualCalibrationAdapter *manualCalibrationAdapter = nullptr;
DeathController *deathController = nullptr;

infra::ArduinoTimeProvider g_timeProvider;
infra::ArduinoRandomSource g_randomSource;

static constexpr const char *TAG = "Main";
static constexpr const char *WIFI_TAG = "WiFi";
static constexpr const char *OTA_TAG = "OTA";
static constexpr const char *DEBUG_TAG = "RemoteDebug";
static constexpr const char *STATE_TAG = "State";
static constexpr const char *AUDIO_TAG = "Audio";
static constexpr const char *BT_TAG = "Bluetooth";
static constexpr const char *FLOW_TAG = "FortuneFlow";
static constexpr const char *LED_TAG = "LED";

bool g_sdCardMounted = false;

String fortunesJsonPath = "/printer/fortunes_littlekid.json";

SDCardContent sdCardContent;
String initializationAudioPath;
bool initializationQueued = false;
bool initializationPlayed = false;
bool remoteDebugStreamingWasEnabled = true;

int32_t IRAM_ATTR provideAudioFramesThunk(void *context, Frame *frame, int32_t frameCount) {
    AudioPlayer *player = static_cast<AudioPlayer *>(context);
    if (!player) {
        return 0;
    }
    return player->provideAudioFrames(frame, frameCount);
}

// Function declarations
void handleUARTCommand(UARTCommand cmd);
void validateAudioDirectories();
void logAudioDirectoryTree(const char *path, int depth = 0);
void playRandomSkit();
void testSkitSelection();
void testCategorySelection(const char *directory, const char *label);
void breathingJawMovement();
void processCLI(String cmd);
void printHelp();
void printSDCardInfo();
void printFingerHelp();
void printFortuneToSerial(const String &fortune);
void updatePrinterFaultIndicator();

unsigned long fingerStableMs = 120;
unsigned long fingerWaitMs = 6000;
unsigned long snapDelayMinMs = 1000;
unsigned long snapDelayMaxMs = 3000;
unsigned long cooldownMs = 12000;

// Fortune flow state
bool mouthOpen = false;
bool mouthPulseActive = false;
bool printerFaultLatched = false;

static constexpr const char *AUDIO_WELCOME_DIR = "/audio/welcome";
static constexpr const char *AUDIO_FINGER_PROMPT_DIR = "/audio/finger_prompt";
static constexpr const char *AUDIO_FINGER_SNAP_DIR = "/audio/finger_snap";
static constexpr const char *AUDIO_NO_FINGER_DIR = "/audio/no_finger";
static constexpr const char *AUDIO_FORTUNE_PREAMBLE_DIR = "/audio/fortune_preamble";
static constexpr const char *AUDIO_GOODBYE_DIR = "/audio/goodbye";
static constexpr const char *AUDIO_FORTUNE_TEMPLATES_DIR = "/audio/fortune_templates";
static constexpr const char *AUDIO_FORTUNE_TOLD_DIR = "/audio/fortune_told";

static constexpr int SERVO_POSITION_MARGIN_DEGREES = 3;

int countWavFilesInDirectory(const char *directory) {
    File dir = SD_MMC.open(directory);
    if (!dir) {
        return -1;
    }
    if (!dir.isDirectory()) {
        dir.close();
        return -1;
    }

    int count = 0;
    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            name.trim();
            if (!name.startsWith(".")) {
                size_t fileSize = entry.size();
                if (fileSize > 0 && (name.endsWith(".wav") || name.endsWith(".WAV"))) {
                    count++;
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return count;
}

std::vector<std::string> gatherFortuneCandidates(SDCardManager *sdManager, const String &configuredPath) {
    std::vector<std::string> result;
    auto pushUnique = [&](const std::string &path) {
        if (path.empty()) {
            return;
        }
        if (std::find(result.begin(), result.end(), path) == result.end()) {
            result.push_back(path);
        }
    };

    std::vector<String> candidates;
    if (configuredPath.length() > 0) {
        candidates.push_back(configuredPath);
    }
    candidates.push_back("/fortunes");
    candidates.push_back("/fortunes/little_kid_fortunes.json");
    candidates.push_back("/printer/fortunes_littlekid.json");

    for (const String &candidateRaw : candidates) {
        String candidate = candidateRaw;
        candidate.trim();
        if (candidate.length() == 0) {
            continue;
        }

        while (candidate.endsWith("/") && candidate.length() > 1) {
            candidate = candidate.substring(0, candidate.length() - 1);
        }

        bool handled = false;
        if (g_sdCardMounted && sdManager) {
            File entry = SD_MMC.open(candidate.c_str());
            if (entry) {
                if (entry.isDirectory()) {
                    std::vector<std::string> files;
                    File child = entry.openNextFile();
                    while (child) {
                        if (!child.isDirectory()) {
                            String name = child.name();
                            if (!name.startsWith(".")) {
                                String fullPath = name;
                                if (!fullPath.startsWith("/")) {
                                    fullPath = candidate + "/" + fullPath;
                                }
                                if (fullPath.endsWith(".json") || fullPath.endsWith(".JSON")) {
                                    files.push_back(fullPath.c_str());
                                }
                            }
                        }
                        child.close();
                        child = entry.openNextFile();
                    }
                    entry.close();
                    for (const auto &path : files) {
                        pushUnique(path);
                    }
                    handled = true;
                } else {
                    entry.close();
                    if (candidate.endsWith(".json") || candidate.endsWith(".JSON")) {
                        pushUnique(candidate.c_str());
                        handled = true;
                    }
                }
            } else {
                if (candidate.endsWith(".json") || candidate.endsWith(".JSON")) {
                    int slashIndex = candidate.lastIndexOf('/');
                    String basename = (slashIndex >= 0) ? candidate.substring(slashIndex + 1) : candidate;
                    String altPath = "/fortunes/" + basename;
                    if (sdManager->fileExists(altPath.c_str())) {
                        pushUnique(std::string(altPath.c_str()));
                        handled = true;
                    }
                }
            }
        }

        if (!handled) {
            pushUnique(std::string(candidate.c_str()));
        }
    }

    return result;
}

void validateAudioDirectories() {
    if (!sdCardManager) {
        return;
    }

    struct DirCheck {
        const char *path;
        const char *description;
        bool optional;
    };

    static const DirCheck checks[] = {
        {AUDIO_WELCOME_DIR, "welcome skits", false},
        {AUDIO_FINGER_PROMPT_DIR, "finger prompt skits", false},
        {AUDIO_FINGER_SNAP_DIR, "finger snap skits", false},
        {AUDIO_NO_FINGER_DIR, "no-finger skits", false},
        {AUDIO_FORTUNE_PREAMBLE_DIR, "fortune preamble skits", false},
        {AUDIO_GOODBYE_DIR, "goodbye skits", false},
        {AUDIO_FORTUNE_TEMPLATES_DIR, "fortune template pools", true},
        {AUDIO_FORTUNE_TOLD_DIR, "fortune told stingers", true},
    };

    LOG_INFO(AUDIO_TAG, "Audio directory validation starting...\n");
    logAudioDirectoryTree("/audio", 0);

    for (const auto &check : checks) {
        int count = countWavFilesInDirectory(check.path);
        if (count <= 0) {
            if (check.optional) {
                LOG_WARN(AUDIO_TAG,
                         "Audio directory '%s' is empty or missing (%s) — OK, this feature is future work.",
                         check.path,
                         check.description);
            } else {
                LOG_WARN(AUDIO_TAG,
                         "Audio directory '%s' is empty or missing (%s). Add at least one .wav clip for production.",
                         check.path,
                         check.description);
            }
        }
    }
}

void logAudioDirectoryTree(const char *path, int depth) {
    if (!path || depth > 6) {
        return; // prevent runaway recursion
    }

    File dir = SD_MMC.open(path);
    if (!dir) {
        LOG_WARN(AUDIO_TAG, "%*s[missing] %s", depth * 2, "", path);
        return;
    }
    if (!dir.isDirectory()) {
        LOG_WARN(AUDIO_TAG, "%*s[not a directory] %s", depth * 2, "", path);
        dir.close();
        return;
    }

    LOG_INFO(AUDIO_TAG, "%*s📁 %s", depth * 2, "", path);

    File entry = dir.openNextFile();
    while (entry) {
        String name = entry.name();
        name.trim();

        if (!name.startsWith(".")) {
            String fullPath = String(path);
            if (!fullPath.endsWith("/")) {
                fullPath += '/';
            }
            fullPath += name;

            if (entry.isDirectory()) {
                logAudioDirectoryTree(fullPath.c_str(), depth + 1);
            } else {
                size_t sizeBytes = entry.size();
                if (sizeBytes == 0) {
                    LOG_WARN(AUDIO_TAG, "%*s⚠️  %s (0 bytes)", depth * 2 + 2, "", fullPath.c_str());
                } else {
                    LOG_INFO(AUDIO_TAG, "%*s🎵 %s (%u bytes)", depth * 2 + 2, "", fullPath.c_str(), static_cast<unsigned>(sizeBytes));
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }

    dir.close();
}

int computeServoMarginDegrees() {
    int minDeg = servoController.getMinDegrees();
    int maxDeg = servoController.getMaxDegrees();
    int span = maxDeg - minDeg;
    if (span <= 2) {
        return 0;
    }

    int margin = SERVO_POSITION_MARGIN_DEGREES;
    if (margin * 2 >= span) {
        margin = span / 4;
    }
    if (margin < 1) {
        margin = 1;
    }
    if (margin * 2 >= span) {
        margin = std::max(1, span / 3);
    }
    if (margin * 2 >= span) {
        margin = std::max(1, span / 4);
    }
    return std::min(margin, span / 2);
}

int getServoClosedPosition() {
    int minDeg = servoController.getMinDegrees();
    int margin = computeServoMarginDegrees();
    int maxDeg = servoController.getMaxDegrees();
    if (margin <= 0 || minDeg + margin >= maxDeg) {
        return minDeg;
    }
    return minDeg + margin;
}

int getServoOpenPosition() {
    int maxDeg = servoController.getMaxDegrees();
    int minDeg = servoController.getMinDegrees();
    int margin = computeServoMarginDegrees();
    if (margin <= 0 || maxDeg - margin <= minDeg) {
        return maxDeg;
    }
    return maxDeg - margin;
}

static void processControllerActions(const DeathController::ControllerActions &actions) {
    if (!deathController) {
        return;
    }

    if (!actions.audioToQueue.empty() && audioPlayer) {
        for (const std::string &clip : actions.audioToQueue) {
            if (clip.empty()) {
                continue;
            }
            LOG_INFO(FLOW_TAG, "Controller queuing audio: %s", clip.c_str());
            audioPlayer->playNext(String(clip.c_str()));
        }
    }

    if (!actions.fortuneText.empty()) {
        String fortuneText(actions.fortuneText.c_str());
        printFortuneToSerial(fortuneText);
    }

    if (actions.requestMouthOpen || actions.requestMouthClose) {
        const int servoOpen = getServoOpenPosition();
        const int servoClosed = getServoClosedPosition();
        if (actions.requestMouthOpen) {
            servoController.setPosition(servoOpen);
            mouthOpen = true;
        } else if (actions.requestMouthClose) {
            servoController.setPosition(servoClosed);
            mouthOpen = false;
        }
    }

    if (actions.requestMouthPulseEnable) {
        lightController.setMouthPulse();
        mouthPulseActive = true;
    }
    if (actions.requestMouthPulseDisable) {
        lightController.setMouthOff();
        mouthPulseActive = false;
    }

    if (actions.requestLedPrompt) {
        lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
        lightController.setMouthBright();
    }
    if (actions.requestLedIdle) {
        lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
        lightController.setMouthOff();
    }
    if (actions.requestLedFingerDetected) {
        lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
        lightController.setMouthBright();
    }

    if (actions.queueFortunePrint) {
        bool success = false;
        if (thermalPrinter) {
            String fortuneText(actions.fortuneText.c_str());
            success = thermalPrinter->queueFortunePrint(fortuneText);
        }
        if (!success) {
            LOG_WARN(FLOW_TAG, "Controller requested fortune print but printer unavailable or failed");
        }
    }


    if (actions.requestRemoteDebugPause && remoteDebugManager) {
        remoteDebugManager->setAutoStreaming(false);
    }
    if (actions.requestRemoteDebugResume && remoteDebugManager) {
        remoteDebugManager->setAutoStreaming(true);
    }
}

void updatePrinterFaultIndicator() {
    if (!thermalPrinter || printerFaultLatched) {
        return;
    }

    if (thermalPrinter->hasError()) {
        printerFaultLatched = true;
        LOG_WARN(LED_TAG, "Printer fault detected; latching eye fault indicator");
        lightController.startEyeBlinkPattern(3,
                                             120,
                                             120,
                                             800,
                                             LightController::BRIGHTNESS_MAX,
                                             LightController::BRIGHTNESS_OFF,
                                             2,
                                             "Printer fault");
    }
}

// Breathing cycle state
unsigned long lastJawMovementTime = 0;
const unsigned long BREATHING_INTERVAL = 7000;        // 7 seconds between breaths
const int BREATHING_JAW_ANGLE = 30;                   // 30 degrees opening
const int BREATHING_MOVEMENT_DURATION = 2000;         // 2 seconds per movement

// Serial command buffer
String cmdBuffer = "";

void setup() {
    Serial.begin(115200);
    delay(500); // Allow serial to stabilize

    LoggingManager::instance().begin(&Serial);

    class LoggingManagerSink : public infra::ILogSink {
    public:
        void log(infra::LogLevel level, const char *tag, const char *message) override {
            LoggingManager::instance().log(static_cast<LogLevel>(level), tag ? tag : TAG, "%s", message ? message : "");
        }
    };
    static LoggingManagerSink loggingSink;
    infra::setLogSink(&loggingSink);
    LOG_INFO(TAG, "💀 Death starting…");

    remoteDebugManager = new RemoteDebugManager();

    auto pauseRemoteDebugForOta = [&]() {
        if (!remoteDebugManager) return;
        remoteDebugStreamingWasEnabled = remoteDebugManager->isAutoStreaming();
        if (remoteDebugStreamingWasEnabled) {
            remoteDebugManager->setAutoStreaming(false);
            remoteDebugManager->println("🛜 RemoteDebug: auto streaming paused during OTA");
        }
    };

    auto restoreRemoteDebugAfterOta = [&](const char *enabledMsg, const char *disabledMsg) {
        if (!remoteDebugManager) return;
        if (remoteDebugStreamingWasEnabled) {
            remoteDebugManager->setAutoStreaming(true);
            remoteDebugManager->println(enabledMsg);
        } else {
            remoteDebugManager->println(disabledMsg);
        }
        remoteDebugStreamingWasEnabled = remoteDebugManager->isAutoStreaming();
    };


    // Normal initialization code continues here...
    lightController.begin();
    lightController.blinkLights(3); // Startup blink

    // Try to mount SD card and load config before initializing servo
    sdCardManager = new SDCardManager();
    audioDirectorySelector = new AudioDirectorySelector();
    audioPlannerAdapter = new AudioPlannerAdapter(*audioDirectorySelector);
    bool configLoaded = false;
    g_sdCardMounted = false;
    ConfigManager &config = ConfigManager::getInstance();
    
    // Try to mount SD card (with retries)
    int sdRetryCount = 0;
    const int MAX_SD_RETRIES = 5;
    while (!sdCardManager->begin() && sdRetryCount < MAX_SD_RETRIES) {
        LOG_WARN(TAG, "⚠️ SD card mount failed! Retrying… (%d/%d)", sdRetryCount + 1, MAX_SD_RETRIES);
        lightController.blinkEyes(3);
        delay(500);
        sdRetryCount++;
    }
    
    if (sdRetryCount < MAX_SD_RETRIES) {
        g_sdCardMounted = true;
        LOG_INFO(TAG, "SD card mounted successfully");
        sdCardContent = sdCardManager->loadContent();
        
        // Try to load config
        int retryCount = 0;
        const int MAX_RETRIES = 5;
        
        while (!config.loadConfig() && retryCount < MAX_RETRIES) {
            LOG_WARN(TAG, "⚠️ Failed to load config. Retrying… (%d/%d)", retryCount + 1, MAX_RETRIES);
            lightController.blinkEyes(5);
            delay(500);
            retryCount++;
        }
        
        if (retryCount < MAX_RETRIES) {
            configLoaded = true;
            LOG_INFO(TAG, "Configuration loaded successfully");
        } else {
            LOG_WARN(TAG, "⚠️ Config failed to load after %d retries", MAX_RETRIES);
        }
    } else {
        LOG_WARN(TAG, "⚠️ SD card mount failed after %d retries - using safe defaults", MAX_SD_RETRIES);
    }
    
    // Initialize servo with config values if loaded, otherwise use safe defaults
    if (configLoaded) {
        fingerStableMs = config.getFingerDetectMs();
        fingerWaitMs = config.getFingerWaitMs();
        snapDelayMinMs = config.getSnapDelayMinMs();
        snapDelayMaxMs = config.getSnapDelayMaxMs();
        cooldownMs = config.getCooldownMs();
        if (snapDelayMinMs > snapDelayMaxMs) {
            unsigned long temp = snapDelayMinMs;
            snapDelayMinMs = snapDelayMaxMs;
            snapDelayMaxMs = temp;
        }
        LOG_INFO(FLOW_TAG, "Timer config — fingerStable=%lums fingerWait=%lums snapDelay=%lu-%lums cooldown=%lums",
                 fingerStableMs, fingerWaitMs, snapDelayMinMs, snapDelayMaxMs, cooldownMs);
        lightController.configureMouthLED(config.getMouthLedBright(),
                                          config.getMouthLedPulseMin(),
                                          config.getMouthLedPulseMax(),
                                          config.getMouthLedPulsePeriodMs());
        int minUs = config.getServoUSMin();
        int maxUs = config.getServoUSMax();
        LOG_INFO(TAG, "Initializing servo with config values: %d-%d µs", minUs, maxUs);
        servoController.initialize(SERVO_PIN, 0, 80, minUs, maxUs);
    } else {
        const int SAFE_MIN_US = 1400;
        const int SAFE_MAX_US = 1600;
        LOG_INFO(TAG, "Initializing servo with safe defaults: %d-%d µs", SAFE_MIN_US, SAFE_MAX_US);
        servoController.initialize(SERVO_PIN, 0, 80, SAFE_MIN_US, SAFE_MAX_US);
    }

    fortunesJsonPath = config.getFortunesJson();
    if (fortunesJsonPath.length() == 0) {
        fortunesJsonPath = "/printer/fortunes_littlekid.json";
    }
    LOG_INFO(FLOW_TAG, "Fortune JSON path: %s", fortunesJsonPath.c_str());
    std::vector<std::string> fortuneCandidates = gatherFortuneCandidates(sdCardManager, fortunesJsonPath);

    String printerLogoPath = config.getPrinterLogo();
    if (printerLogoPath.length() == 0) {
        printerLogoPath = "/printer/logo_384w.bmp";
    }
    LOG_INFO(FLOW_TAG, "Printer logo path: %s", printerLogoPath.c_str());

    initializationAudioPath = "/audio/initialized.wav";
    LOG_INFO(AUDIO_TAG, "🎵 Initialization audio: %s", initializationAudioPath.c_str());

    audioPlayer = new AudioPlayer(*sdCardManager);
    if (audioPlannerAdapter) {
        audioPlannerAdapter->setAudioPlayer(audioPlayer);
    }
    audioPlayer->setPlaybackStartCallback([](const String &filePath) {
        LOG_INFO(AUDIO_TAG, "▶️ Audio playback started: %s", filePath.c_str());
        if (deathController) {
            deathController->handleAudioStarted(std::string(filePath.c_str()));
            processControllerActions(deathController->pendingActions());
            deathController->clearActions();
        }
        if (filePath.equals(initializationAudioPath)) {
            initializationQueued = false;
        }
    });
    audioPlayer->setPlaybackEndCallback([](const String &filePath) {
        LOG_INFO(AUDIO_TAG, "⏹ Audio playback finished: %s", filePath.c_str());
        if (deathController) {
            deathController->handleAudioFinished(std::string(filePath.c_str()));
            processControllerActions(deathController->pendingActions());
            deathController->clearActions();
            return;
        }
        if (filePath.equals(initializationAudioPath)) {
            initializationPlayed = true;
        }
        if (skullAudioAnimator) {
            skullAudioAnimator->setPlaybackEnded(filePath);
        }
        // Update skit selector with the completed skit
        if (skitSelector && filePath.startsWith("/audio/Skit")) {
            skitSelector->updateSkitPlayCount(filePath);
        }
    });
    audioPlayer->setAudioFramesProvidedCallback([](const String &filePath, const Frame *frames, int32_t frameCount) {
        if (skullAudioAnimator && frameCount > 0) {
            unsigned long playbackTime = audioPlayer->getPlaybackTime();
            skullAudioAnimator->processAudioFrames(frames, frameCount, filePath, playbackTime);
        }
    });

    bool bluetoothEnabledConfig = config.isBluetoothEnabled();
#ifdef DISABLE_BLUETOOTH
    bluetoothEnabledConfig = false;
    bluetoothController = nullptr;
    LOG_WARN(BT_TAG, "Bluetooth disabled for this build (DISABLE_BLUETOOTH set)");
#else
    if (bluetoothEnabledConfig) {
        bluetoothController = new BluetoothController();
    } else {
        bluetoothController = nullptr;
        LOG_WARN(BT_TAG, "Bluetooth disabled via config (bluetooth_enabled=false)");
    }
#endif

    uartController = new UARTController(MATTER_RX_PIN, MATTER_TX_PIN);
    if (uartController) {
        uartController->begin();
    }
    thermalPrinter = new ThermalPrinter(Serial2, PRINTER_TX_PIN, PRINTER_RX_PIN, config.getPrinterBaud());
    fingerSensor = new FingerSensor(CAP_SENSE_PIN);
    fortuneGenerator = new FortuneGenerator();
    if (fortuneGenerator) {
        fortuneServiceAdapter = new FortuneServiceAdapter(*fortuneGenerator);
    }
    if (thermalPrinter) {
        printerStatusAdapter = new PrinterStatusAdapter(*thermalPrinter);
    }
    if (fingerSensor) {
        manualCalibrationAdapter = new ManualCalibrationAdapter(lightController, *fingerSensor, ConfigManager::getInstance());
    }

    if (thermalPrinter) {
        thermalPrinter->setLogoPath(printerLogoPath);
        thermalPrinter->begin();
    } else {
        LOG_WARN(FLOW_TAG, "Thermal printer allocation failed");
    }

    if (fingerSensor) {
        fingerSensor->setTouchCycles(config.getFingerCyclesInit(), config.getFingerCyclesMeasure());
        fingerSensor->setFilterAlpha(config.getFingerFilterAlpha());
        fingerSensor->setBaselineDrift(config.getFingerBaselineDrift());
        fingerSensor->setMultisampleCount(config.getFingerMultisample());
        fingerSensor->setSensitivity(config.getCapThreshold());
        fingerSensor->begin();
        fingerSensor->setStableDurationMs(fingerStableMs);
        fingerSensor->setStreamIntervalMs(500);
    } else {
        LOG_WARN(FLOW_TAG, "Finger sensor allocation failed");
    }

    if (!deathController && audioPlannerAdapter && fortuneServiceAdapter) {
        DeathController::Dependencies controllerDeps{};
        controllerDeps.time = &g_timeProvider;
        controllerDeps.random = &g_randomSource;
        controllerDeps.log = infra::getLogSink();
        controllerDeps.audioPlanner = audioPlannerAdapter;
        controllerDeps.fortuneService = fortuneServiceAdapter;
        controllerDeps.printerStatus = printerStatusAdapter;
        controllerDeps.manualCalibDriver = manualCalibrationAdapter;
        deathController = new DeathController(controllerDeps);

        DeathController::ConfigSnapshot snapshot;
        snapshot.fingerStableMs = fingerStableMs;
        snapshot.fingerWaitMs = fingerWaitMs;
        snapshot.snapDelayMinMs = snapDelayMinMs;
        snapshot.snapDelayMaxMs = snapDelayMaxMs;
        snapshot.cooldownMs = cooldownMs;
        snapshot.welcomeDir = AUDIO_WELCOME_DIR;
        snapshot.fingerPromptDir = AUDIO_FINGER_PROMPT_DIR;
        snapshot.fingerSnapDir = AUDIO_FINGER_SNAP_DIR;
        snapshot.noFingerDir = AUDIO_NO_FINGER_DIR;
        snapshot.fortunePreambleDir = AUDIO_FORTUNE_PREAMBLE_DIR;
        if (!fortuneCandidates.empty()) {
            snapshot.fortuneFlowDir = fortuneCandidates.front();
        } else {
            snapshot.fortuneFlowDir = fortunesJsonPath.c_str();
        }
        snapshot.fortuneCandidates = fortuneCandidates;
        snapshot.fortuneDoneDir = AUDIO_FORTUNE_TOLD_DIR;
        deathController->initialize(snapshot);
        deathController->clearActions();
    }

    const int servoMinDegrees = 0;
    const int servoMaxDegrees = 80;
    // This skull is always the primary/coordinator animatronic
    const bool isPrimary = true;
    skullAudioAnimator = new SkullAudioAnimator(isPrimary, servoController, lightController, sdCardContent.skits, *sdCardManager,
                                                servoMinDegrees, servoMaxDegrees);
    skullAudioAnimator->setSpeakingStateCallback([](bool isSpeaking) {
        lightController.setEyeBrightness(isSpeaking ? LightController::BRIGHTNESS_MAX : LightController::BRIGHTNESS_DIM);
    });

    // Initialize skit selector for preventing immediate repeats
    skitSelector = new SkitSelector(sdCardContent.skits);
    
    // Test skit selection to verify repeat prevention works
    testSkitSelection();
    testCategorySelection(AUDIO_WELCOME_DIR, "welcome skit");
    testCategorySelection(AUDIO_FORTUNE_PREAMBLE_DIR, "fortune preamble");
    validateAudioDirectories();
    
    LOG_INFO(TAG, "✅ All components initialized successfully");

    wifiManager = new WiFiManager();
    otaManager = new OTAManager();

    if (otaManager) {
        otaManager->setOnStartCallback([&]() {
            LOG_INFO(OTA_TAG, "⏸️ OTA start: pausing peripherals");
            pauseRemoteDebugForOta();
            if (audioPlayer) {
                audioPlayer->setMuted(true);
            }
            if (bluetoothController) {
                bluetoothController->pauseForOta();
            }
        });
        otaManager->setOnEndCallback([&]() {
            LOG_INFO(OTA_TAG, "▶️ OTA completed: resuming peripherals");
            restoreRemoteDebugAfterOta("🛜 RemoteDebug: auto streaming resumed after OTA", "🛜 RemoteDebug: auto streaming left disabled after OTA");
            if (audioPlayer) {
                audioPlayer->setMuted(false);
            }
            if (bluetoothController) {
                bluetoothController->resumeAfterOta();
            }
        });
        otaManager->setOnErrorCallback([&](ota_error_t error) {
            LOG_WARN(OTA_TAG, "⚠️ OTA aborted: resuming peripherals");
            restoreRemoteDebugAfterOta("🛜 RemoteDebug: auto streaming resumed after OTA abort", "🛜 RemoteDebug: auto streaming left disabled after OTA abort");
            if (audioPlayer) {
                audioPlayer->setMuted(false);
            }
            if (bluetoothController && !bluetoothController->isRetryingConnection()) {
                bluetoothController->resumeAfterOta();
            }
        });
    }

    String wifiSSID = config.getWiFiSSID();
    String wifiPassword = config.getWiFiPassword();
    String otaHostname = config.getOTAHostname();
    String otaPassword = config.getOTAPassword();

    LOG_INFO(WIFI_TAG, "🛜 Checking Wi-Fi configuration…");
    LOG_INFO(WIFI_TAG, "   SSID: %s", wifiSSID.length() > 0 ? wifiSSID.c_str() : "[NOT SET]");
    LOG_INFO(WIFI_TAG, "   Password: %s", wifiPassword.length() > 0 ? "[SET]" : "[NOT SET]");
    LOG_INFO(WIFI_TAG, "   OTA Hostname: %s", otaHostname.c_str());
    LOG_INFO(WIFI_TAG, "   OTA Password: %s", otaPassword.length() > 0 ? "[SET]" : "[NOT SET]");

    if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
        LOG_INFO(WIFI_TAG, "Initializing Wi-Fi manager");
        wifiManager->setHostname(otaHostname);
        wifiManager->setConnectionCallback([wifiSSID, otaHostname, otaPassword](bool connected) {
            if (connected) {
                    LOG_INFO(WIFI_TAG, "🛜 Connected! SSID: %s, IP: %s", wifiSSID.c_str(), wifiManager->getIPAddress().c_str());
                if (remoteDebugManager) {
                    if (remoteDebugManager->begin(23)) {
                        LOG_INFO(DEBUG_TAG, "🛜 Telnet server started on port 23 (telnet %s 23)", wifiManager->getIPAddress().c_str());
                    } else {
                        LOG_ERROR(DEBUG_TAG, "Failed to start telnet server");
                    }
                }
                if (otaManager && !otaManager->isEnabled()) {
                    if (otaManager->begin(otaHostname, otaPassword)) {
                        LOG_INFO(OTA_TAG, "🔄 OTA manager started (port 3232)");
                        LOG_INFO(OTA_TAG, "🔐 OTA password protection enabled");
                    } else if (otaManager->disabledForMissingPassword()) {
                        LOG_ERROR(OTA_TAG, "OTA password missing; OTA disabled");
                    } else {
                        LOG_ERROR(OTA_TAG, "❌ OTA manager failed to start");
                    }
                }
            } else {
                LOG_WARN(WIFI_TAG, "⚠️ Wi-Fi connection failed");
            }
        });
        wifiManager->setDisconnectionCallback([]() {
            LOG_WARN(WIFI_TAG, "⚠️ Wi-Fi disconnected");
        });
        if (wifiManager->begin(wifiSSID, wifiPassword)) {
            LOG_INFO(WIFI_TAG, "Wi-Fi manager started, attempting connection…");
        } else {
            LOG_ERROR(WIFI_TAG, "❌ Wi-Fi manager failed to start");
        }
    } else {
        LOG_WARN(WIFI_TAG, "⚠️ Wi-Fi credentials incomplete or missing; wireless features disabled");
    }

    if (bluetoothController && bluetoothEnabledConfig) {
        bluetoothController->initializeA2DP(config.getBluetoothSpeakerName(), provideAudioFramesThunk, audioPlayer);
        bluetoothController->setConnectionStateChangeCallback([](int state) {
            if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                bool isPlaying = audioPlayer && audioPlayer->isAudioPlaying();
                bool hasQueue = audioPlayer && audioPlayer->hasQueuedAudio();
                LOG_INFO(BT_TAG, "🔗 Bluetooth speaker connected. initPlayed=%s, isAudioPlaying=%s, hasQueued=%s",
                         initializationPlayed ? "true" : "false",
                         isPlaying ? "true" : "false",
                         hasQueue ? "true" : "false");
                if (!initializationPlayed && audioPlayer && !initializationQueued) {
                    LOG_INFO(BT_TAG, "🎬 Priming initialization audio after Bluetooth connect");
                    audioPlayer->playNext(initializationAudioPath);
                    initializationQueued = true;
                }
            } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                LOG_WARN(BT_TAG, "🔌 Bluetooth speaker disconnected");
            }
        });
        bluetoothController->set_volume(config.getSpeakerVolume());
        bluetoothController->startConnectionRetry();
    }

    if (initializationAudioPath.length() > 0 && sdCardManager->fileExists(initializationAudioPath.c_str())) {
        audioPlayer->playNext(initializationAudioPath);
        LOG_INFO(AUDIO_TAG, "🎵 Queued initialization audio: %s", initializationAudioPath.c_str());
        initializationQueued = true;
        initializationPlayed = false;
    } else {
        LOG_WARN(AUDIO_TAG, "⚠️ Initialization audio missing: %s", initializationAudioPath.c_str());
    }

    LOG_INFO(TAG, "🎉 Death initialized successfully");
}

void loop() {
    unsigned long currentTime = millis();
    bool controllerActive = (deathController != nullptr);

    // Update audio player (fills buffer, handles playback events)
    if (audioPlayer) {
        audioPlayer->update();
    }

    // Update Bluetooth controller (processes media start, connection retry)
    if (bluetoothController) {
        bluetoothController->update();
    }

    if (fingerSensor) {
        // NOTE(2025-11-05): Finger sensor currently reports no touch on hardware – likely physical wiring damage.
        fingerSensor->update();
    }

    if (thermalPrinter) {
        thermalPrinter->update();
        updatePrinterFaultIndicator();
    }

    lightController.update();

    if (uartController) {
        uartController->update();
        UARTCommand lastCommand = uartController->getLastCommand();
        if (lastCommand != UARTCommand::NONE) {
            handleUARTCommand(lastCommand);
            uartController->clearLastCommand();
        }
        
        // Report handshake status periodically
        static unsigned long lastHandshakeReport = 0;
        if (currentTime - lastHandshakeReport > 30000) { // Every 30 seconds
            bool bootComplete = uartController->isBootHandshakeComplete();
            bool fabricComplete = uartController->isFabricHandshakeComplete();
            LOG_INFO(STATE_TAG, "UART Handshake Status - Boot: %s, Fabric: %s", 
                     bootComplete ? "OK" : "PENDING", 
                     fabricComplete ? "OK" : "PENDING");
            lastHandshakeReport = currentTime;
        }
    }
    
    // Check if it's time to move the jaw for breathing / update controller
    if (controllerActive) {
        DeathController::FingerReadout readout{};
        if (fingerSensor) {
            readout.detected = fingerSensor->isFingerDetected();
            readout.stable = fingerSensor->hasStableTouch();
            readout.normalizedDelta = fingerSensor->getNormalizedDelta();
            readout.thresholdRatio = fingerSensor->getThresholdRatio();
        }
        deathController->update(currentTime, readout);
        processControllerActions(deathController->pendingActions());
        deathController->clearActions();
        bool fingerStreaming = fingerSensor && fingerSensor->isStreamEnabled();
        bool controllerIdle = deathController->state() == DeathController::State::Idle;
        if (!fingerStreaming && audioPlayer && controllerIdle &&
            currentTime - lastJawMovementTime >= BREATHING_INTERVAL && !audioPlayer->isAudioPlaying()) {
            breathingJawMovement();
            lastJawMovementTime = currentTime;
        }
    }
    
    // Handle serial commands
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cmdBuffer.length() > 0) {
                processCLI(cmdBuffer);
                cmdBuffer = "";
            }
        } else {
            cmdBuffer += c;
        }
    }
}

void handleUARTCommand(UARTCommand cmd) {
    LOG_INFO(STATE_TAG, "Handling UART command: %s", UARTController::commandToString(cmd));

    if (cmd == UARTCommand::LEGACY_PING || cmd == UARTCommand::LEGACY_SET_MODE) {
        LOG_WARN(STATE_TAG, "Legacy UART command ignored: %s", UARTController::commandToString(cmd));
        return;
    }

    if (cmd == UARTCommand::BOOT_HELLO || cmd == UARTCommand::FABRIC_HELLO) {
        LOG_INFO(STATE_TAG, "Handshake command processed: %s", UARTController::commandToString(cmd));
    }

    if (!deathController) {
        LOG_WARN(STATE_TAG, "DeathController not initialized; command ignored");
        return;
    }

    deathController->handleUartCommand(cmd);
    processControllerActions(deathController->pendingActions());
    deathController->clearActions();
}

void printFortuneToSerial(const String &fortune) {
    Serial.println();
    Serial.println(F("=== FORTUNE ==="));

    if (fortune.length() == 0) {
        Serial.println(F("(empty fortune)"));
    } else {
        const size_t chunkSize = 96;
        size_t len = fortune.length();
        size_t offset = 0;
        while (offset < len) {
            size_t end = std::min(offset + chunkSize, len);
            Serial.println(fortune.substring(offset, end));
            offset = end;
        }
    }

    Serial.println(F("================"));
    Serial.println();
}

void playRandomSkit() {
    if (skitSelector && audioPlayer) {
        ParsedSkit selectedSkit = skitSelector->selectNextSkit();
        if (!selectedSkit.audioFile.isEmpty()) {
            LOG_INFO(FLOW_TAG, "Playing random skit: %s", selectedSkit.audioFile.c_str());
            audioPlayer->playNext(selectedSkit.audioFile);
        } else {
            LOG_WARN(FLOW_TAG, "No skits available for selection");
        }
    } else {
        LOG_WARN(FLOW_TAG, "SkitSelector or AudioPlayer not available");
    }
}

void testSkitSelection() {
    if (!skitSelector) {
        LOG_WARN(FLOW_TAG, "SkitSelector not available for testing");
        return;
    }
    
    LOG_INFO(FLOW_TAG, "Testing skit selection (repeat prevention)...");
    
    // Test first selection to check if any skits are available
    ParsedSkit firstSkit = skitSelector->selectNextSkit();
    if (firstSkit.audioFile.isEmpty()) {
        LOG_WARN(FLOW_TAG, "No skits available for testing");
        return;
    }
    
    LOG_INFO(FLOW_TAG, "Test selection 1: %s", firstSkit.audioFile.c_str());
    
    // Test multiple selections to verify no immediate repeats
    for (int i = 1; i < 5; i++) {
        ParsedSkit selectedSkit = skitSelector->selectNextSkit();
        if (selectedSkit.audioFile.isEmpty()) {
            LOG_WARN(FLOW_TAG, "No skits available for test selection %d", i + 1);
            break;
        }
        LOG_INFO(FLOW_TAG, "Test selection %d: %s", i + 1, selectedSkit.audioFile.c_str());
        delay(100); // Small delay to ensure different timestamps
    }
    
    LOG_INFO(FLOW_TAG, "Skit selection test completed");
}

void testCategorySelection(const char *directory, const char *label) {
    if (!audioDirectorySelector) {
        LOG_WARN(AUDIO_TAG, "Audio selector unavailable; skipping %s category test", label ? label : "(unknown)");
        return;
    }

    const int available = countWavFilesInDirectory(directory);
    if (available <= 0) {
        LOG_WARN(AUDIO_TAG, "Skipping %s selection test — available clips: %d", label ? label : "(unknown)", available);
        audioDirectorySelector->resetStats(directory);
        return;
    }

    const int iterations = available > 1 ? 4 : 1;
    String last;
    bool repeatDetected = false;

    for (int i = 0; i < iterations; ++i) {
        String clip = audioDirectorySelector->selectClip(directory, label);
        if (clip.isEmpty()) {
            LOG_WARN(AUDIO_TAG, "Selection returned empty for %s on iteration %d", label ? label : "(unknown)", i + 1);
            repeatDetected = true;
            break;
        }
        if (!last.isEmpty() && clip == last && available > 1) {
            LOG_WARN(AUDIO_TAG, "Immediate repeat detected for %s: %s", label ? label : "(unknown)", clip.c_str());
            repeatDetected = true;
            break;
        }
        last = clip;
        delay(25);
    }

    if (!repeatDetected) {
        LOG_INFO(AUDIO_TAG, "Category selector validation passed for %s (clips=%d)", label ? label : "(unknown)", available);
    }

    audioDirectorySelector->resetStats(directory);
}

void breathingJawMovement() {
    if (!audioPlayer->isAudioPlaying()) {
        int closedPosition = getServoClosedPosition();
        int openTarget = closedPosition + BREATHING_JAW_ANGLE;
        openTarget = std::min(openTarget, getServoOpenPosition());

        servoController.smoothMove(openTarget, BREATHING_MOVEMENT_DURATION);
        delay(100); // Short pause at open position
        servoController.smoothMove(closedPosition, BREATHING_MOVEMENT_DURATION);
    }
}

void printHelp() {
    Serial.println("\n=== DEATH FORTUNE TELLER CLI ===");
    Serial.println("Core commands:");
    Serial.println("  help            - Show this help");
    Serial.println("  config          - Show configuration settings");
    Serial.println("  sd              - Show SD card content info");
    Serial.println("Servo diagnostics:");
    Serial.println("  sinit           - Run servo initialization sweep");
    Serial.println("  smin            - Move servo to MIN (0°)");
    Serial.println("  smin <µs>       - Set MIN to specified µs (500-10000)");
    Serial.println("  smin +/-        - Adjust MIN by ±100 µs");
    Serial.println("  smax            - Move servo to MAX (80°)");
    Serial.println("  smax <µs>       - Set MAX to specified µs (500-10000)");
    Serial.println("  smax +/-        - Adjust MAX by ±100 µs");
    Serial.println("  scfg            - Show servo configuration");
    Serial.println("  smic <µs>       - Set servo pulse width directly (test range)");
    Serial.println("  sdeg <deg>      - Set servo position in degrees (0-80)");
    Serial.println("  srev            - Toggle servo direction reversal\n");
    Serial.println("Printer diagnostics:");
    Serial.println("  ptest          - Trigger printer self-test page");
    Serial.println();

    Serial.println("Finger sensor tuning:");
    Serial.println("  cal / fcal      - Recalibrate finger sensor baseline");
    Serial.println("  fhelp           - Finger sensor tuning help");
    Serial.println("  fstatus         - Show live readings snapshot");
    Serial.println("  fsettings       - Show current tuning values");
    Serial.println("  fsens <0-1>     - Set adaptive sensitivity margin (higher = less sensitive)");
    Serial.println("  fthresh <0-1>   - Set minimum normalized threshold clamp (alias: thresh)");
    Serial.println("  fdebounce <ms>  - Set detection hold duration (30-1000)");
    Serial.println("  finterval <ms>  - Set streaming interval (100-10000)");
    Serial.println("  fcycles <init> <meas> - Set touchSetCycles values (hex/dec)");
    Serial.println("  falpha <0-1>    - Set smoothing alpha (0=slow,1=fast)");
    Serial.println("  fdrift <0-0.1>  - Set baseline drift factor");
    Serial.println("  fmultisample <N>- Set sample averaging count (>=1)");
    Serial.println("  fon / foff      - Enable or disable continuous streaming");
    Serial.println();
}

void printFingerHelp() {
    Serial.println("\n=== FINGER SENSOR TUNING ===");
    Serial.println("Commands:");
    Serial.println("  fhelp             - Show this help");
    Serial.println("  fcal              - Recalibrate finger sensor (1s baseline)");
    Serial.println("  fsens <0-1>       - Set adaptive sensitivity margin (higher = less sensitive)");
    Serial.println("  fthresh <0-1>     - Set minimum normalized threshold clamp (alias: thresh)");
    Serial.println("  fdebounce <ms>    - Set required stable detection duration");
    Serial.println("  finterval <ms>    - Set streaming interval (100-10000 ms)");
    Serial.println("  fon / foff        - Enable/disable live touch stream");
    Serial.println("  fstatus           - Show current readings");
    Serial.println("  fsettings         - Show current tuning values");
    Serial.println("  fcycles <init> <meas> - Set touchSetCycles initial/measure");
    Serial.println("  falpha <0-1>      - Set smoothing alpha");
    Serial.println("  fdrift <0-0.1>    - Set baseline drift factor");
    Serial.println("  fmultisample <N>  - Set sample averaging count");
    Serial.println();
}

void printSDCardInfo() {
    Serial.println("\n=== SD CARD CONTENT ===");
    Serial.print("Skits loaded:     "); Serial.println(sdCardContent.skits.size());
    Serial.print("Audio files:      "); Serial.println(sdCardContent.audioFiles.size());
    
    if (sdCardContent.skits.size() > 0) {
        Serial.println("\nSkits:");
        for (size_t i = 0; i < sdCardContent.skits.size() && i < 10; i++) {
            Serial.print("  "); Serial.print(i + 1); Serial.print(". "); Serial.println(sdCardContent.skits[i].audioFile);
        }
        if (sdCardContent.skits.size() > 10) {
            Serial.print("  ... and "); Serial.print(sdCardContent.skits.size() - 10); Serial.println(" more");
        }
    }
    Serial.println();
}

void processCLI(String cmd) {
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "help" || cmd == "?") {
        printHelp();
    }
    else if (cmd == "fhelp" || cmd == "f?") {
        printFingerHelp();
    }
    else if (cmd == "cal" || cmd == "calibrate" || cmd == "fcal" || cmd == "fcalibrate") {
        Serial.println("\n>>> Calibrating finger sensor...");
        Serial.println(">>> REMOVE YOUR FINGER NOW!");
        if (fingerSensor) {
            fingerSensor->calibrate();
            Serial.println(">>> Calibration running (watch logs for completion)...\n");
        } else {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
        }
    }
    else if (cmd == "fsens" || cmd.startsWith("fsens ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        String valuePart = cmd.length() > 5 ? cmd.substring(5) : "";
        valuePart.trim();
        if (valuePart.length() == 0) {
            Serial.print(">>> Current sensitivity margin: ");
            Serial.print(fingerSensor->getSensitivity(), 3);
            Serial.print(" (noise delta ");
            Serial.print(fingerSensor->getNoiseNormalized() * 100.0f, 3);
            Serial.println("%)\n");
            return;
        }
        float val = valuePart.toFloat();
        if (val < 0.0f || val > 1.0f) {
            Serial.println(">>> ERROR: Sensitivity must be between 0.0 and 1.0\n");
            return;
        }
        if (fingerSensor->setSensitivity(val)) {
            Serial.print(">>> Sensitivity margin set to ");
            Serial.print(val, 3);
            Serial.print(" (effective threshold ");
            Serial.print(fingerSensor->getThresholdRatio() * 100.0f, 3);
            Serial.println("%)\n");
        } else {
            Serial.println(">>> ERROR: Failed to set sensitivity\n");
        }
    }
    else if (cmd == "thresh" || cmd.startsWith("thresh ") ||
             cmd == "fthresh" || cmd.startsWith("fthresh ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        String valuePart;
        if (cmd.startsWith("fthresh")) {
            valuePart = cmd.substring(strlen("fthresh"));
        } else {
            valuePart = cmd.substring(strlen("thresh"));
        }
        valuePart.trim();
        if (valuePart.length() == 0) {
            Serial.println(">>> ERROR: Missing value. Use e.g. fthresh 0.005\n");
            return;
        }

        float val = valuePart.toFloat();
        if (val <= 0.0f || val > 1.0f) {
            Serial.println(">>> ERROR: Threshold must be between 0 and 1 (e.g., 0.002)\n");
            return;
        }
        if (fingerSensor->setThresholdRatio(val)) {
            Serial.print(">>> Minimum threshold clamp set to ");
            Serial.print(val * 100.0f, 3);
            Serial.println("%\n");
        } else {
            Serial.println(">>> ERROR: Threshold out of supported range (0.0001 - 1.0)\n");
        }
    }
    else if (cmd.startsWith("fdebounce ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        unsigned long val = cmd.substring(10).toInt();
        if (fingerSensor->setStableDurationMs(val)) {
            fingerStableMs = val;
            Serial.print(">>> Stable detection duration set to ");
            Serial.print(val);
            Serial.println(" ms\n");
        } else {
            Serial.println(">>> ERROR: Duration must be between 30 and 1000 ms\n");
        }
    }
    else if (cmd.startsWith("finterval ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        unsigned long val = cmd.substring(10).toInt();
        if (fingerSensor->setStreamIntervalMs(val)) {
            Serial.print(">>> Stream interval set to ");
            Serial.print(val);
            Serial.println(" ms\n");
        } else {
            Serial.println(">>> ERROR: Interval must be between 100 and 10000 ms\n");
        }
    }
    else if (cmd == "fon") {
        if (fingerSensor) {
            fingerSensor->setStreamEnabled(true);
            Serial.println(">>> Finger sensor stream enabled\n");
        } else {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
        }
    }
    else if (cmd == "foff") {
        if (fingerSensor) {
            fingerSensor->setStreamEnabled(false);
            Serial.println(">>> Finger sensor stream disabled\n");
        } else {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
        }
    }
    else if (cmd == "fstatus") {
        if (fingerSensor) {
            fingerSensor->printStatus(Serial);
        } else {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
        }
    }
    else if (cmd == "fsettings") {
        if (fingerSensor) {
            fingerSensor->printSettings(Serial);
        } else {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
        }
    }
    else if (cmd.startsWith("fcycles ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        String args = cmd.substring(strlen("fcycles"));
        args.trim();
        int spaceIdx = args.indexOf(' ');
        if (spaceIdx <= 0) {
            Serial.println(">>> ERROR: Usage fcycles <initial> <measure>\n");
            return;
        }
        String initStr = args.substring(0, spaceIdx);
        String measStr = args.substring(spaceIdx + 1);
        uint16_t initVal = static_cast<uint16_t>(strtol(initStr.c_str(), nullptr, 0));
        uint16_t measVal = static_cast<uint16_t>(strtol(measStr.c_str(), nullptr, 0));
        if (fingerSensor->setTouchCycles(initVal, measVal)) {
            Serial.print(">>> touchSetCycles updated to init=0x");
            Serial.print(initVal, HEX);
            Serial.print(" measure=0x");
            Serial.print(measVal, HEX);
            Serial.println("\n");
        } else {
            Serial.println(">>> ERROR: Invalid cycle values (must be >0)\n");
        }
    }
    else if (cmd.startsWith("falpha ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        float val = cmd.substring(7).toFloat();
        if (fingerSensor->setFilterAlpha(val)) {
            Serial.print(">>> Filter alpha set to ");
            Serial.println(val, 4);
            Serial.println();
        } else {
            Serial.println(">>> ERROR: Alpha must be within 0.0 - 1.0\n");
        }
    }
    else if (cmd.startsWith("fdrift ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        float val = cmd.substring(7).toFloat();
        if (fingerSensor->setBaselineDrift(val)) {
            Serial.print(">>> Baseline drift set to ");
            Serial.println(val, 6);
            Serial.println();
        } else {
            Serial.println(">>> ERROR: Drift must be within 0.0 - 0.1\n");
        }
    }
    else if (cmd.startsWith("fmultisample ")) {
        if (!fingerSensor) {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
            return;
        }
        int val = cmd.substring(12).toInt();
        if (fingerSensor->setMultisampleCount(static_cast<uint8_t>(val))) {
            Serial.print(">>> Multisample count set to ");
            Serial.println(val);
            Serial.println();
        } else {
            Serial.println(">>> ERROR: Count must be >= 1\n");
        }
    }
    else if (cmd == "config" || cmd == "settings") {
        Serial.println("\n=== CONFIGURATION SETTINGS ===");
        ConfigManager &config = ConfigManager::getInstance();
        config.printConfig();
        Serial.println();
    }
    else if (cmd == "sd" || cmd == "sdcard") {
        printSDCardInfo();
    }
    else if (cmd == "ptest") {
        if (!thermalPrinter) {
            Serial.println(">>> ERROR: Thermal printer not initialized\n");
        } else if (!thermalPrinter->isReady()) {
            Serial.println(">>> ERROR: Thermal printer not ready (check power/paper)\n");
        } else if (thermalPrinter->printTestPage()) {
            Serial.println(">>> Printer self-test initiated. Check the diagnostic printout.\n");
        } else {
            Serial.println(">>> ERROR: Failed to start printer self-test\n");
        }
    }
    else if (cmd == "sinit") {
        Serial.println("\n>>> Running servo initialization sweep...");
        if (servoController.getPosition() >= 0) {
            int minUs = servoController.getMinMicroseconds();
            int maxUs = servoController.getMaxMicroseconds();
            int minDeg = servoController.getMinDegrees();
            int maxDeg = servoController.getMaxDegrees();
            Serial.printf(">>> Config: degrees %d-%d, microseconds %d-%d µs\n", minDeg, maxDeg, minUs, maxUs);
            servoController.reattachWithConfigLimits();
            Serial.println(">>> Servo sweep complete!\n");
        } else {
            Serial.println(">>> ERROR: Servo not initialized\n");
        }
    }
    else if (cmd.startsWith("smin")) {
        String valuePart = cmd.substring(4);
        valuePart.trim();
        
        if (cmd == "smin") {
            // No args: move to MIN
            Serial.println("\n>>> Moving servo to MIN position...");
            if (servoController.getPosition() >= 0) {
                int minDeg = servoController.getMinDegrees();
                int minUs = servoController.getMinMicroseconds();
                Serial.printf(">>> Moving to MIN: %d° (%d µs)\n", minDeg, minUs);
                servoController.smoothMove(minDeg, 500);
                Serial.printf(">>> Servo moved to MIN: %d degrees (%d µs)\n\n", minDeg, minUs);
            } else {
                Serial.println(">>> ERROR: Servo not initialized\n");
            }
        } else if (valuePart == "+") {
            // smin +: add 100 to min
            int newMin = servoController.getMinMicroseconds() + 100;
            servoController.setMinMicroseconds(newMin);
            Serial.printf("\n>>> MIN increased to %d µs\n\n", newMin);
        } else if (valuePart == "-") {
            // smin -: subtract 100 from min
            int newMin = servoController.getMinMicroseconds() - 100;
            servoController.setMinMicroseconds(newMin);
            Serial.printf("\n>>> MIN decreased to %d µs\n\n", newMin);
        } else if (valuePart.length() > 0) {
            // smin <value>: set min to specified value
            int newMin = valuePart.toInt();
            if (newMin >= 500 && newMin <= 10000) {
                servoController.setMinMicroseconds(newMin);
                Serial.printf("\n>>> MIN set to %d µs\n\n", newMin);
            } else {
                Serial.println(">>> ERROR: Value must be 500-10000 µs\n");
            }
        }
    }
    else if (cmd.startsWith("smax")) {
        String valuePart = cmd.substring(4);
        valuePart.trim();
        
        if (cmd == "smax") {
            // No args: move to MAX
            Serial.println("\n>>> Moving servo to MAX position...");
            if (servoController.getPosition() >= 0) {
                int maxDeg = servoController.getMaxDegrees();
                int maxUs = servoController.getMaxMicroseconds();
                Serial.printf(">>> Moving to MAX: %d° (%d µs)\n", maxDeg, maxUs);
                servoController.smoothMove(maxDeg, 500);
                Serial.printf(">>> Servo moved to MAX: %d degrees (%d µs)\n\n", maxDeg, maxUs);
            } else {
                Serial.println(">>> ERROR: Servo not initialized\n");
            }
        } else if (valuePart == "+") {
            // smax +: add 100 to max
            int newMax = servoController.getMaxMicroseconds() + 100;
            servoController.setMaxMicroseconds(newMax);
            Serial.printf("\n>>> MAX increased to %d µs\n\n", newMax);
        } else if (valuePart == "-") {
            // smax -: subtract 100 from max
            int newMax = servoController.getMaxMicroseconds() - 100;
            servoController.setMaxMicroseconds(newMax);
            Serial.printf("\n>>> MAX decreased to %d µs\n\n", newMax);
        } else if (valuePart.length() > 0) {
            // smax <value>: set max to specified value
            int newMax = valuePart.toInt();
            if (newMax >= 500 && newMax <= 10000) {
                servoController.setMaxMicroseconds(newMax);
                Serial.printf("\n>>> MAX set to %d µs\n\n", newMax);
            } else {
                Serial.println(">>> ERROR: Value must be 500-10000 µs\n");
            }
        }
    }
    else if (cmd == "scfg") {
        Serial.println("\n>>> Servo Configuration:");
        Serial.printf("  Pin: %d\n", SERVO_PIN);
        Serial.printf("  Degree range: %d-%d\n", servoController.getMinDegrees(), servoController.getMaxDegrees());
        Serial.printf("  Pulse width range: %d-%d µs\n", servoController.getMinMicroseconds(), servoController.getMaxMicroseconds());
        Serial.printf("  Current MIN: %d µs | MAX: %d µs\n", servoController.getMinMicroseconds(), servoController.getMaxMicroseconds());
        Serial.printf("  Current position: %d degrees\n", servoController.getPosition());
        Serial.printf("  Direction reversed: %s\n", servoController.isReversed() ? "YES" : "NO");
        Serial.println();
    }
    else if (cmd.startsWith("smic ")) {
        String valuePart = cmd.substring(5);
        valuePart.trim();
        if (valuePart.length() == 0) {
            Serial.println(">>> ERROR: Specify microseconds (500-2500)\n");
        } else {
            int us = valuePart.toInt();
            if (us < 500 || us > 2500) {
                Serial.println(">>> ERROR: Pulse width must be 500-2500 µs\n");
            } else {
                servoController.writeMicroseconds(us);
                Serial.printf(">>> Servo set to %d µs (pulse width)\n\n", us);
            }
        }
    }
    else if (cmd.startsWith("sdeg ")) {
        String valuePart = cmd.substring(5);
        valuePart.trim();
        if (valuePart.length() == 0) {
            Serial.println(">>> ERROR: Specify degrees (0-80)\n");
        } else {
            int deg = valuePart.toInt();
            if (deg < 0 || deg > 80) {
                Serial.println(">>> ERROR: Degrees must be 0-80\n");
            } else {
                servoController.setPosition(deg);
                Serial.printf(">>> Servo set to %d degrees\n\n", deg);
            }
        }
    }
    else if (cmd == "srev") {
        bool wasReversed = servoController.isReversed();
        servoController.setReverseDirection(!wasReversed);
        Serial.printf("\n>>> Servo direction reversal %s\n", (!wasReversed ? "ENABLED" : "DISABLED"));
        Serial.printf("    Current state: %s\n", servoController.isReversed() ? "REVERSED" : "NORMAL");
        Serial.println();
    }
    else if (cmd.length() > 0) {
        Serial.print(">>> Unknown command: "); Serial.print(cmd); Serial.println(". Type 'help' for commands.\n");
    }

}
