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
#include "skit_selector.h"
#include "audio_directory_selector.h"
#include <WiFi.h>
#include "SD_MMC.h"
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "esp_attr.h"
#include <vector>
#include <algorithm>

// Pin definitions
const int EYE_LED_PIN = 32;    // GPIO pin for eye LED
const int MOUTH_LED_PIN = 33;  // GPIO pin for mouth LED
const int SERVO_PIN = 23;      // Servo control pin (moved off SD CMD line)
const int CAP_SENSE_PIN = 4;   // Capacitive finger sensor pin
const int PRINTER_TX_PIN = 21; // Thermal printer TX pin
const int PRINTER_RX_PIN = 20; // Thermal printer RX pin

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

static constexpr const char *TAG = "Main";
static constexpr const char *WIFI_TAG = "WiFi";
static constexpr const char *OTA_TAG = "OTA";
static constexpr const char *DEBUG_TAG = "RemoteDebug";
static constexpr const char *STATE_TAG = "State";
static constexpr const char *AUDIO_TAG = "Audio";
static constexpr const char *BT_TAG = "Bluetooth";
static constexpr const char *FLOW_TAG = "FortuneFlow";

bool g_sdCardMounted = false;

String fortunesJsonPath = "/printer/fortunes_littlekid.json";
bool fortuneGeneratorLoadAttempted = false;
bool fortuneGeneratorLoaded = false;
String activeFortune;
String fortuneSourceFile;
bool fortuneGenerationAttempted = false;
bool fortunePrintAttempted = false;
bool fortunePrintSuccess = false;

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

enum class DeathState {
    IDLE,
    PLAY_WELCOME,
    WAIT_FOR_NEAR,
    PLAY_FINGER_PROMPT,
    MOUTH_OPEN_WAIT_FINGER,
    FINGER_DETECTED,
    SNAP_WITH_FINGER,
    SNAP_NO_FINGER,
    FORTUNE_FLOW,
    FORTUNE_DONE,
    COOLDOWN
};

// Function declarations
void handleUARTCommand(UARTCommand cmd);
void enterState(DeathState newState, const char *reason, bool forced = false);
void updateStateMachine(unsigned long currentTime);
void handleAudioPlaybackFinished(const String &filePath);
bool queueRandomClipFromDir(const char *directory, const char *description);
void validateAudioDirectories();
void logAudioDirectoryTree(const char *path, int depth = 0);
void playRandomSkit();
void testSkitSelection();
void testCategorySelection(const char *directory, const char *label);
void breathingJawMovement();
void processCLI(String cmd);
void printHelp();
void printSDCardInfo();
void resetFortuneFlowWork();
bool ensureFortuneGeneratorLoaded();
void generateAndPrintFortune();
void printFortuneToSerial(const String &fortune);

DeathState currentState = DeathState::IDLE;
unsigned long stateStartTime = 0;
unsigned long lastTriggerTime = 0;
const unsigned long TRIGGER_DEBOUNCE_MS = 2000;

unsigned long fingerWaitMs = 6000;
unsigned long snapDelayMinMs = 1000;
unsigned long snapDelayMaxMs = 3000;
unsigned long cooldownMs = 12000;
const unsigned long FINGER_STABLE_MS = 120;

// Fortune flow state
bool mouthOpen = false;
bool fingerDetected = false;
unsigned long fingerDetectionStart = 0;
unsigned long fingerWaitStart = 0;
unsigned long snapDelayStart = 0;
unsigned long cooldownStart = 0;
int snapDelayMs = 0;
bool snapDelayScheduled = false;

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

bool queueRandomClipFromDir(const char *directory, const char *description) {
    if (!audioPlayer) {
        LOG_WARN(AUDIO_TAG, "AudioPlayer not initialized; cannot queue %s", description ? description : "clip");
        return false;
    }

    if (!audioDirectorySelector) {
        LOG_WARN(AUDIO_TAG, "Audio selector not initialized; cannot queue %s", description ? description : "clip");
        return false;
    }

    String selected = audioDirectorySelector->selectClip(directory, description);
    if (selected.isEmpty()) {
        return false;
    }

    audioPlayer->playNext(selected);
    LOG_INFO(AUDIO_TAG, "Queued audio for %s: %s", description ? description : "clip", selected.c_str());
    return true;
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

bool isTriggerCommand(UARTCommand cmd) {
    return cmd == UARTCommand::FAR_MOTION_TRIGGER || cmd == UARTCommand::NEAR_MOTION_TRIGGER;
}

bool isForcedStateCommand(UARTCommand cmd) {
    switch (cmd) {
        case UARTCommand::PLAY_WELCOME:
        case UARTCommand::WAIT_FOR_NEAR:
        case UARTCommand::PLAY_FINGER_PROMPT:
        case UARTCommand::MOUTH_OPEN_WAIT_FINGER:
        case UARTCommand::FINGER_DETECTED:
        case UARTCommand::SNAP_WITH_FINGER:
        case UARTCommand::SNAP_NO_FINGER:
        case UARTCommand::FORTUNE_FLOW:
        case UARTCommand::FORTUNE_DONE:
        case UARTCommand::COOLDOWN:
            return true;
        default:
            return false;
    }
}

const char *deathStateToString(DeathState state) {
    switch (state) {
        case DeathState::IDLE: return "IDLE";
        case DeathState::PLAY_WELCOME: return "PLAY_WELCOME";
        case DeathState::WAIT_FOR_NEAR: return "WAIT_FOR_NEAR";
        case DeathState::PLAY_FINGER_PROMPT: return "PLAY_FINGER_PROMPT";
        case DeathState::MOUTH_OPEN_WAIT_FINGER: return "MOUTH_OPEN_WAIT_FINGER";
        case DeathState::FINGER_DETECTED: return "FINGER_DETECTED";
        case DeathState::SNAP_WITH_FINGER: return "SNAP_WITH_FINGER";
        case DeathState::SNAP_NO_FINGER: return "SNAP_NO_FINGER";
        case DeathState::FORTUNE_FLOW: return "FORTUNE_FLOW";
        case DeathState::FORTUNE_DONE: return "FORTUNE_DONE";
        case DeathState::COOLDOWN: return "COOLDOWN";
        default: return "UNKNOWN";
    }
}

DeathState stateForCommand(UARTCommand cmd) {
    switch (cmd) {
        case UARTCommand::PLAY_WELCOME: return DeathState::PLAY_WELCOME;
        case UARTCommand::WAIT_FOR_NEAR: return DeathState::WAIT_FOR_NEAR;
        case UARTCommand::PLAY_FINGER_PROMPT: return DeathState::PLAY_FINGER_PROMPT;
        case UARTCommand::MOUTH_OPEN_WAIT_FINGER: return DeathState::MOUTH_OPEN_WAIT_FINGER;
        case UARTCommand::FINGER_DETECTED: return DeathState::FINGER_DETECTED;
        case UARTCommand::SNAP_WITH_FINGER: return DeathState::SNAP_WITH_FINGER;
        case UARTCommand::SNAP_NO_FINGER: return DeathState::SNAP_NO_FINGER;
        case UARTCommand::FORTUNE_FLOW: return DeathState::FORTUNE_FLOW;
        case UARTCommand::FORTUNE_DONE: return DeathState::FORTUNE_DONE;
        case UARTCommand::COOLDOWN: return DeathState::COOLDOWN;
        default: return DeathState::IDLE;
    }
}

bool isBusy() {
    return currentState != DeathState::IDLE;
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
        fingerWaitMs = config.getFingerWaitMs();
        snapDelayMinMs = config.getSnapDelayMinMs();
        snapDelayMaxMs = config.getSnapDelayMaxMs();
        cooldownMs = config.getCooldownMs();
        if (snapDelayMinMs > snapDelayMaxMs) {
            unsigned long temp = snapDelayMinMs;
            snapDelayMinMs = snapDelayMaxMs;
            snapDelayMaxMs = temp;
        }
        LOG_INFO(FLOW_TAG, "Timer config — fingerWait=%lums snapDelay=%lu-%lums cooldown=%lums",
                 fingerWaitMs, snapDelayMinMs, snapDelayMaxMs, cooldownMs);
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

    initializationAudioPath = "/audio/initialized.wav";
    LOG_INFO(AUDIO_TAG, "🎵 Initialization audio: %s", initializationAudioPath.c_str());

    audioPlayer = new AudioPlayer(*sdCardManager);
    audioPlayer->setPlaybackStartCallback([](const String &filePath) {
        LOG_INFO(AUDIO_TAG, "▶️ Audio playback started: %s", filePath.c_str());
        if (filePath.equals(initializationAudioPath)) {
            initializationQueued = false;
        }
    });
    audioPlayer->setPlaybackEndCallback([](const String &filePath) {
        LOG_INFO(AUDIO_TAG, "⏹ Audio playback finished: %s", filePath.c_str());
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
        handleAudioPlaybackFinished(filePath);
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

    uartController = new UARTController();
    if (uartController) {
        uartController->begin();
    }
    thermalPrinter = new ThermalPrinter(Serial2, PRINTER_TX_PIN, PRINTER_RX_PIN, config.getPrinterBaud());
    fingerSensor = new FingerSensor(CAP_SENSE_PIN);
    fortuneGenerator = new FortuneGenerator();

    if (thermalPrinter) {
        thermalPrinter->begin();
    } else {
        LOG_WARN(FLOW_TAG, "Thermal printer allocation failed");
    }

    if (fingerSensor) {
        fingerSensor->begin();
    } else {
        LOG_WARN(FLOW_TAG, "Finger sensor allocation failed");
    }

    if (fortuneGenerator) {
        ensureFortuneGeneratorLoaded();
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

    enterState(DeathState::IDLE, "Boot initialization", true);
    LOG_INFO(TAG, "🎉 Death initialized successfully");
}

void loop() {
    // Update audio player (fills buffer, handles playback events)
    if (audioPlayer) {
        audioPlayer->update();
    }
    
    // Update Bluetooth controller (processes media start, connection retry)
    if (bluetoothController) {
        bluetoothController->update();
    }

    if (fingerSensor) {
        fingerSensor->update();
    }

    if (thermalPrinter) {
        thermalPrinter->update();
    }

    if (uartController) {
        uartController->update();
        UARTCommand lastCommand = uartController->getLastCommand();
        if (lastCommand != UARTCommand::NONE) {
            handleUARTCommand(lastCommand);
            uartController->clearLastCommand();
        }
    }
    
    // Check if it's time to move the jaw for breathing
    unsigned long currentTime = millis();
    updateStateMachine(currentTime);
    if (audioPlayer && currentState == DeathState::IDLE &&
        currentTime - lastJawMovementTime >= BREATHING_INTERVAL && !audioPlayer->isAudioPlaying()) {
        breathingJawMovement();
        lastJawMovementTime = currentTime;
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
    unsigned long currentTime = millis();
    if (isTriggerCommand(cmd)) {
        if (currentTime - lastTriggerTime < TRIGGER_DEBOUNCE_MS) {
            LOG_WARN(STATE_TAG, "Trigger command debounced");
            return;
        }

        if (cmd == UARTCommand::FAR_MOTION_TRIGGER) {
            if (isBusy()) {
                LOG_WARN(STATE_TAG, "Ignoring FAR trigger while busy (state=%s)", deathStateToString(currentState));
                return;
            }
            enterState(DeathState::PLAY_WELCOME, "FAR trigger");
            lastTriggerTime = currentTime;
            return;
        }

        if (cmd == UARTCommand::NEAR_MOTION_TRIGGER) {
            if (currentState != DeathState::WAIT_FOR_NEAR) {
                LOG_WARN(STATE_TAG, "NEAR trigger dropped - current state %s (must be WAIT_FOR_NEAR)", deathStateToString(currentState));
                return;
            }
            enterState(DeathState::PLAY_FINGER_PROMPT, "NEAR trigger");
            lastTriggerTime = currentTime;
            return;
        }
    }

    if (isForcedStateCommand(cmd)) {
        DeathState target = stateForCommand(cmd);
        LOG_WARN(STATE_TAG, "State forcing command received: %s -> %s", UARTController::commandToString(cmd), deathStateToString(target));
        enterState(target, "Forced via Matter command", true);
        return;
    }

    if (cmd == UARTCommand::LEGACY_PING || cmd == UARTCommand::LEGACY_SET_MODE) {
        LOG_WARN(STATE_TAG, "Legacy UART command ignored: %s", UARTController::commandToString(cmd));
        return;
    }

    LOG_WARN(STATE_TAG, "Unhandled UART command: %s", UARTController::commandToString(cmd));
}

void resetFortuneFlowWork() {
    activeFortune = "";
    fortuneGenerationAttempted = false;
    fortunePrintAttempted = false;
    fortunePrintSuccess = false;
}

bool ensureFortuneGeneratorLoaded() {
    if (!fortuneGenerator) {
        return false;
    }

    if (fortuneGeneratorLoaded) {
        return true;
    }

    if (!g_sdCardMounted) {
        if (!fortuneGeneratorLoadAttempted) {
            LOG_WARN(FLOW_TAG, "Cannot load fortunes: SD card not mounted");
        }
        fortuneGeneratorLoadAttempted = true;
        return false;
    }

    if (!sdCardManager) {
        if (!fortuneGeneratorLoadAttempted) {
            LOG_WARN(FLOW_TAG, "SD card manager unavailable; cannot load fortunes");
        }
        fortuneGeneratorLoadAttempted = true;
        return false;
    }

    std::vector<String> candidates;
    if (fortunesJsonPath.length() > 0) {
        candidates.push_back(fortunesJsonPath);
    }
    candidates.push_back("/fortunes");
    candidates.push_back("/fortunes/little_kid_fortunes.json");
    candidates.push_back("/printer/fortunes_littlekid.json");

    String selectedPath;

    for (const auto &candidateRaw : candidates) {
        String candidate = candidateRaw;
        candidate.trim();
        if (candidate.length() == 0) {
            continue;
        }

        // Normalize path (remove trailing slash except root)
        if (candidate.endsWith("/") && candidate.length() > 1) {
            candidate = candidate.substring(0, candidate.length() - 1);
        }

        File entry = SD_MMC.open(candidate.c_str());
        if (entry) {
            if (entry.isDirectory()) {
                std::vector<String> files;
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
                                files.push_back(fullPath);
                            }
                        }
                    }
                    child.close();
                    child = entry.openNextFile();
                }
                entry.close();

                if (!files.empty()) {
                    size_t index = files.size() == 1 ? 0 : static_cast<size_t>(random(files.size()));
                    selectedPath = files[index];
                    LOG_INFO(FLOW_TAG, "Fortune directory %s selected %s", candidate.c_str(), selectedPath.c_str());
                    break;
                } else {
                    LOG_WARN(FLOW_TAG, "No fortune JSON files found in %s", candidate.c_str());
                }
            } else {
                entry.close();
                if (!candidate.endsWith(".json") && !candidate.endsWith(".JSON")) {
                    LOG_WARN(FLOW_TAG, "Configured fortune file is not JSON: %s", candidate.c_str());
                }
                selectedPath = candidate;
                break;
            }
        } else {
            // Attempt alternate location if config path is stale
            if (candidate.endsWith(".json") || candidate.endsWith(".JSON")) {
                int slashIndex = candidate.lastIndexOf('/');
                String basename = (slashIndex >= 0) ? candidate.substring(slashIndex + 1) : candidate;
                String altPath = "/fortunes/" + basename;
                if (sdCardManager->fileExists(altPath.c_str())) {
                    LOG_INFO(FLOW_TAG, "Fortune file %s missing; using %s instead", candidate.c_str(), altPath.c_str());
                    selectedPath = altPath;
                    break;
                }
            }
        }
    }

    if (selectedPath.length() == 0) {
        if (!fortuneGeneratorLoadAttempted) {
            LOG_WARN(FLOW_TAG, "No fortune JSON file found; falling back to default fortune");
        }
        fortuneGeneratorLoadAttempted = true;
        return false;
    }

    fortuneGeneratorLoadAttempted = true;
    fortuneGeneratorLoaded = fortuneGenerator->loadFortunes(selectedPath);
    if (fortuneGeneratorLoaded) {
        fortuneSourceFile = selectedPath;
        LOG_INFO(FLOW_TAG, "Fortune templates loaded from %s", selectedPath.c_str());
    } else {
        LOG_ERROR(FLOW_TAG, "Failed to load fortune templates from %s", selectedPath.c_str());
    }
    return fortuneGeneratorLoaded;
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

void generateAndPrintFortune() {
    if (fortuneGenerationAttempted) {
        return;
    }

    fortuneGenerationAttempted = true;

    if (!fortuneGenerator) {
        activeFortune = "The spirits are silent...";
        LOG_WARN(FLOW_TAG, "Fortune generator unavailable; using fallback fortune");
    } else {
        ensureFortuneGeneratorLoaded();
        activeFortune = fortuneGenerator->generateFortune();
        if (activeFortune.length() == 0) {
            activeFortune = "The spirits are silent...";
        }
    }

    LOG_INFO(FLOW_TAG, "Generated fortune: %s", activeFortune.c_str());
    printFortuneToSerial(activeFortune);

    fortunePrintAttempted = true;

    if (!thermalPrinter) {
        fortunePrintSuccess = false;
        LOG_WARN(FLOW_TAG, "Thermal printer unavailable; fortune will not be printed");
        return;
    }

    if (!thermalPrinter->isReady()) {
        fortunePrintSuccess = false;
        LOG_WARN(FLOW_TAG, "Thermal printer not ready; skipping fortune print");
        return;
    }

    fortunePrintSuccess = thermalPrinter->printFortune(activeFortune);
    if (fortunePrintSuccess) {
        LOG_INFO(FLOW_TAG, "Thermal printer started printing fortune");
    } else {
        LOG_ERROR(FLOW_TAG, "Thermal printer failed to print fortune");
    }
}

void enterState(DeathState newState, const char *reason, bool forced) {
    DeathState previous = currentState;
    const char *reasonText = (reason && reason[0]) ? reason : "no reason provided";

    if (previous == newState && !forced) {
        LOG_INFO(STATE_TAG, "State %s already active; ignoring transition request (%s)", deathStateToString(newState), reasonText);
        return;
    }

    LOG_INFO(STATE_TAG, "Transition %s -> %s%s (%s)",
             deathStateToString(previous),
             deathStateToString(newState),
             forced ? " [forced]" : "",
             reasonText);

    currentState = newState;
    stateStartTime = millis();

    if (newState != DeathState::FINGER_DETECTED) {
        fingerDetected = false;
        fingerDetectionStart = 0;
    }
    snapDelayScheduled = false;
    snapDelayMs = 0;
    snapDelayStart = 0;

    const int servoClosed = getServoClosedPosition();
    const int servoOpen = getServoOpenPosition();

    switch (newState) {
        case DeathState::IDLE:
            resetFortuneFlowWork();
            mouthOpen = false;
            servoController.setPosition(servoClosed);
            lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
            fingerWaitStart = 0;
            cooldownStart = 0;
            break;

        case DeathState::PLAY_WELCOME:
            resetFortuneFlowWork();
            mouthOpen = false;
            if (!queueRandomClipFromDir(AUDIO_WELCOME_DIR, "welcome skit")) {
                enterState(DeathState::WAIT_FOR_NEAR, "Welcome audio missing; advancing", forced);
                return;
            }
            lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
            break;

        case DeathState::WAIT_FOR_NEAR:
            mouthOpen = false;
            lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
            break;

        case DeathState::PLAY_FINGER_PROMPT:
            if (!queueRandomClipFromDir(AUDIO_FINGER_PROMPT_DIR, "finger prompt")) {
                enterState(DeathState::MOUTH_OPEN_WAIT_FINGER, "Finger prompt audio missing; advancing", forced);
                return;
            }
            lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
            break;

        case DeathState::MOUTH_OPEN_WAIT_FINGER:
            servoController.setPosition(servoOpen);
            mouthOpen = true;
            fingerWaitStart = millis();
            fingerDetected = false;
            fingerDetectionStart = 0;
            lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
            break;

        case DeathState::FINGER_DETECTED:
            fingerDetected = true;
            mouthOpen = true;
            snapDelayMs = snapDelayMinMs == snapDelayMaxMs ? snapDelayMinMs
                                                           : static_cast<int>(random(snapDelayMinMs, snapDelayMaxMs + 1));
            snapDelayStart = millis();
            snapDelayScheduled = true;
            LOG_INFO(FLOW_TAG, "Finger detected; snap delay %d ms", snapDelayMs);
            break;

        case DeathState::SNAP_WITH_FINGER:
            servoController.setPosition(servoClosed);
            mouthOpen = false;
            if (!queueRandomClipFromDir(AUDIO_FINGER_SNAP_DIR, "snap with finger")) {
                enterState(DeathState::FORTUNE_FLOW, "Snap-with-finger audio missing; skipping", forced);
                return;
            }
            break;

        case DeathState::SNAP_NO_FINGER:
            servoController.setPosition(servoClosed);
            mouthOpen = false;
            if (!queueRandomClipFromDir(AUDIO_NO_FINGER_DIR, "no-finger response")) {
                enterState(DeathState::FORTUNE_FLOW, "Snap-no-finger audio missing; skipping", forced);
                return;
            }
            break;

        case DeathState::FORTUNE_FLOW:
            servoController.setPosition(servoOpen);
            mouthOpen = true;
            lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
            generateAndPrintFortune();
            if (!queueRandomClipFromDir(AUDIO_FORTUNE_PREAMBLE_DIR, "fortune preamble")) {
                enterState(DeathState::FORTUNE_DONE, "Fortune audio missing; advancing", forced);
                return;
            }
            break;

        case DeathState::FORTUNE_DONE:
            mouthOpen = false;
            servoController.setPosition(servoClosed);
            if (!queueRandomClipFromDir(AUDIO_GOODBYE_DIR, "goodbye skit")) {
                enterState(DeathState::COOLDOWN, "Fortune done audio missing; advancing", forced);
                return;
            }
            break;

        case DeathState::COOLDOWN:
            mouthOpen = false;
            servoController.setPosition(servoClosed);
            lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
            cooldownStart = stateStartTime;
            if (fortunePrintAttempted) {
                LOG_INFO(FLOW_TAG, "Fortune cycle summary — printed=%s text=\"%s\"",
                         fortunePrintSuccess ? "true" : "false",
                         activeFortune.c_str());
            }
            break;
    }
}

void updateStateMachine(unsigned long currentTime) {
    switch (currentState) {
        case DeathState::MOUTH_OPEN_WAIT_FINGER: {
            if (fingerSensor && fingerSensor->isFingerDetected()) {
                if (!fingerDetected) {
                    fingerDetected = true;
                    fingerDetectionStart = currentTime;
                    LOG_INFO(FLOW_TAG, "Finger contact detected");
                } else if (currentTime - fingerDetectionStart >= FINGER_STABLE_MS) {
                    enterState(DeathState::FINGER_DETECTED, "Finger stabilized");
                    return;
                }
            } else {
                if (fingerDetected && currentTime - fingerDetectionStart >= FINGER_STABLE_MS) {
                    LOG_INFO(FLOW_TAG, "Finger removed before stabilization");
                }
                fingerDetected = false;
                fingerDetectionStart = 0;
            }

            if (fingerWaitStart > 0 && currentTime - fingerWaitStart >= fingerWaitMs) {
                enterState(DeathState::SNAP_NO_FINGER, "Finger wait timeout");
                return;
            }
            break;
        }

        case DeathState::FINGER_DETECTED: {
            if (!snapDelayScheduled) {
                snapDelayMs = snapDelayMinMs == snapDelayMaxMs ? snapDelayMinMs
                                                               : static_cast<int>(random(snapDelayMinMs, snapDelayMaxMs + 1));
                snapDelayStart = currentTime;
                snapDelayScheduled = true;
                LOG_INFO(FLOW_TAG, "Snap delay scheduled (late): %d ms", snapDelayMs);
            }

            if (snapDelayScheduled && snapDelayMs > 0 &&
                currentTime - snapDelayStart >= static_cast<unsigned long>(snapDelayMs)) {
                enterState(DeathState::SNAP_WITH_FINGER, "Snap delay elapsed");
                return;
            }

            if (fingerSensor && !fingerSensor->isFingerDetected()) {
                static unsigned long lastFingerRemovedWarning = 0;
                const unsigned long FINGER_REMOVED_WARNING_INTERVAL = 1000; // Only warn once per second
                if (currentTime - lastFingerRemovedWarning >= FINGER_REMOVED_WARNING_INTERVAL) {
                    LOG_WARN(FLOW_TAG, "Finger removed after detection; continuing countdown");
                    lastFingerRemovedWarning = currentTime;
                }
            }
            break;
        }

        case DeathState::COOLDOWN:
            if (currentTime - stateStartTime >= cooldownMs) {
                enterState(DeathState::IDLE, "Cooldown elapsed");
            }
            break;

        default:
            break;
    }
}

void handleAudioPlaybackFinished(const String &filePath) {
    (void)filePath;

    switch (currentState) {
        case DeathState::PLAY_WELCOME:
            enterState(DeathState::WAIT_FOR_NEAR, "Welcome audio finished");
            break;

        case DeathState::PLAY_FINGER_PROMPT:
            enterState(DeathState::MOUTH_OPEN_WAIT_FINGER, "Finger prompt finished");
            break;

        case DeathState::SNAP_WITH_FINGER:
        case DeathState::SNAP_NO_FINGER:
            enterState(DeathState::FORTUNE_FLOW, "Snap sequence finished");
            break;

        case DeathState::FORTUNE_FLOW:
            enterState(DeathState::FORTUNE_DONE, "Fortune flow audio finished");
            break;

        case DeathState::FORTUNE_DONE:
            enterState(DeathState::COOLDOWN, "Fortune done sequence complete");
            break;

        default:
            break;
    }
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
    Serial.println("Commands:");
    Serial.println("  help           - Show this help");
    Serial.println("  cal            - Recalibrate finger sensor");
    Serial.println("  config         - Show configuration settings");
    Serial.println("  sd             - Show SD card content info");
    Serial.println("  servo_init     - Run servo initialization sweep");
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
    else if (cmd == "cal" || cmd == "calibrate") {
        Serial.println("\n>>> Calibrating finger sensor...");
        Serial.println(">>> REMOVE YOUR FINGER NOW!");
        if (fingerSensor) {
            fingerSensor->calibrate();
            Serial.println(">>> Calibration complete!\n");
        } else {
            Serial.println(">>> ERROR: Finger sensor not initialized\n");
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
    else if (cmd == "servo_init") {
        Serial.println("\n>>> Running servo initialization sweep...");
        if (servoController.getPosition() >= 0) { // Check if servo is initialized
            servoController.reattachWithConfigLimits();
            Serial.println(">>> Servo sweep complete!\n");
        } else {
            Serial.println(">>> ERROR: Servo not initialized\n");
        }
    }
    else if (cmd.length() > 0) {
        Serial.print(">>> Unknown command: "); Serial.print(cmd); Serial.println(". Type 'help' for commands.\n");
    }
}
