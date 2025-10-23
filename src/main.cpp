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
#include <SD.h>
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "esp_attr.h"

// Pin definitions
const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED
const int SERVO_PIN = 15;      // Servo control pin
const int MOUTH_LED_PIN = 2;   // Mouth LED pin
const int CAP_SENSE_PIN = 4;   // Capacitive finger sensor pin
const int PRINTER_TX_PIN = 21; // Thermal printer TX pin
const int PRINTER_RX_PIN = 20; // Thermal printer RX pin

// Global objects
LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);
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

static constexpr const char *TAG = "Main";
static constexpr const char *WIFI_TAG = "WiFi";
static constexpr const char *OTA_TAG = "OTA";
static constexpr const char *DEBUG_TAG = "RemoteDebug";
static constexpr const char *STATE_TAG = "State";
static constexpr const char *AUDIO_TAG = "Audio";
static constexpr const char *BT_TAG = "Bluetooth";
static constexpr const char *FLOW_TAG = "FortuneFlow";

SDCardContent sdCardContent;
bool isPrimary = true;
String initializationAudioPath;
bool initializationQueued = false;
bool initializationPlayed = false;

int32_t IRAM_ATTR provideAudioFramesThunk(void *context, Frame *frame, int32_t frameCount) {
    AudioPlayer *player = static_cast<AudioPlayer *>(context);
    if (!player) {
        return 0;
    }
    return player->provideAudioFrames(frame, frameCount);
}

// Function declarations
void handleUARTCommand(UARTCommand cmd);
void startWelcomeSequence();
void startFortuneFlow();
void handleFortuneFlow(unsigned long currentTime);

// State machine
enum class DeathState {
    IDLE,
    PLAY_WELCOME,
    FORTUNE_FLOW,
    COOLDOWN
};

DeathState currentState = DeathState::IDLE;
unsigned long stateStartTime = 0;
unsigned long lastTriggerTime = 0;
const unsigned long TRIGGER_DEBOUNCE_MS = 2000;

// Fortune flow state
bool mouthOpen = false;
bool fingerDetected = false;
unsigned long fingerDetectionStart = 0;
unsigned long snapDelayStart = 0;
int snapDelayMs = 0;

void setup() {
    Serial.begin(115200);
    delay(500); // Allow serial to stabilize

    Serial.println("Setup Serial pre-logging");
    Serial.flush();
    delay(100);

    LoggingManager::instance().begin(&Serial);
    LOG_INFO(TAG, "💀 Death starting…");

    remoteDebugManager = new RemoteDebugManager();

    lightController.begin();
    lightController.blinkEyes(3); // Startup blink

    servoController.initialize(SERVO_PIN, 0, 80);

    sdCardManager = new SDCardManager();
    while (!sdCardManager->begin()) {
        LOG_WARN(TAG, "⚠️ SD card mount failed! Retrying…");
        lightController.blinkEyes(3);
        delay(500);
    }
    LOG_INFO(TAG, "SD card mounted successfully");
    sdCardContent = sdCardManager->loadContent();

    ConfigManager &config = ConfigManager::getInstance();
    while (!config.loadConfig()) {
        LOG_WARN(TAG, "⚠️ Failed to load config. Retrying…");
        lightController.blinkEyes(5);
        delay(500);
    }
    LOG_INFO(TAG, "Configuration loaded successfully");

    String role = config.getRole();
    if (role.length() == 0 || role.equalsIgnoreCase("primary")) {
        isPrimary = true;
        LOG_INFO(STATE_TAG, "Role configured as PRIMARY");
    } else if (role.equalsIgnoreCase("secondary")) {
        isPrimary = false;
        LOG_INFO(STATE_TAG, "Role configured as SECONDARY");
    } else {
        LOG_WARN(STATE_TAG, "⚠️ Unknown role '%s'. Defaulting to PRIMARY.", role.c_str());
        isPrimary = true;
    }

    initializationAudioPath = isPrimary ? sdCardContent.primaryInitAudio : sdCardContent.secondaryInitAudio;
    if (initializationAudioPath.length() == 0 && sdCardContent.primaryInitAudio.length() > 0) {
        initializationAudioPath = sdCardContent.primaryInitAudio;
    }
    if (initializationAudioPath.length() == 0) {
        initializationAudioPath = "/audio/Initialized - Primary.wav";
        LOG_INFO(AUDIO_TAG, "🎵 Initialization audio fallback: /audio/Initialized - Primary.wav");
    } else {
        LOG_INFO(AUDIO_TAG, "🎵 Initialization audio discovered: %s", initializationAudioPath.c_str());
    }

    const char *JAW_SYNC_TEST_AUDIO = "/audio/Skit - imitations.wav";
    LOG_INFO(AUDIO_TAG, "🔬 Jaw sync test override: %s", JAW_SYNC_TEST_AUDIO);
    initializationAudioPath = JAW_SYNC_TEST_AUDIO;

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
    });
    audioPlayer->setAudioFramesProvidedCallback([](const String &filePath, const Frame *frames, int32_t frameCount) {
        if (skullAudioAnimator && frameCount > 0) {
            unsigned long playbackTime = audioPlayer->getPlaybackTime();
            skullAudioAnimator->processAudioFrames(frames, frameCount, filePath, playbackTime);
        }
    });

    bluetoothController = new BluetoothController();
    uartController = new UARTController();
    thermalPrinter = new ThermalPrinter(PRINTER_TX_PIN, PRINTER_RX_PIN);
    fingerSensor = new FingerSensor(CAP_SENSE_PIN);
    fortuneGenerator = new FortuneGenerator();

    const int servoMinDegrees = 0;
    const int servoMaxDegrees = 80;
    skullAudioAnimator = new SkullAudioAnimator(isPrimary, servoController, lightController, sdCardContent.skits, *sdCardManager,
                                                servoMinDegrees, servoMaxDegrees);
    skullAudioAnimator->setSpeakingStateCallback([](bool isSpeaking) {
        lightController.setEyeBrightness(isSpeaking ? LightController::BRIGHTNESS_MAX : LightController::BRIGHTNESS_DIM);
    });
    LOG_INFO(TAG, "✅ All components initialized successfully");

    wifiManager = new WiFiManager();
    otaManager = new OTAManager();

    if (otaManager) {
        otaManager->setOnStartCallback([]() {
            LOG_INFO(OTA_TAG, "⏸️ OTA start: pausing peripherals");
            if (audioPlayer) {
                audioPlayer->setMuted(true);
            }
            if (bluetoothController) {
                bluetoothController->stopConnectionRetry();
            }
        });
        otaManager->setOnEndCallback([]() {
            LOG_INFO(OTA_TAG, "▶️ OTA completed: resuming peripherals");
            if (audioPlayer) {
                audioPlayer->setMuted(false);
            }
            if (bluetoothController) {
                bluetoothController->startConnectionRetry();
            }
        });
        otaManager->setOnErrorCallback([](ota_error_t /*error*/) {
            LOG_WARN(OTA_TAG, "⚠️ OTA aborted: resuming peripherals");
            if (audioPlayer) {
                audioPlayer->setMuted(false);
            }
            if (bluetoothController && !bluetoothController->isRetryingConnection()) {
                bluetoothController->startConnectionRetry();
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
    static unsigned long lastStatusLogTime = 0;

    if (bluetoothController) bluetoothController->update();
    if (audioPlayer) audioPlayer->update();
    if (uartController) uartController->update();
    if (thermalPrinter) thermalPrinter->update();
    if (fingerSensor) fingerSensor->update();
    if (wifiManager) wifiManager->update();
    if (otaManager) otaManager->update();
    if (remoteDebugManager) remoteDebugManager->update();

    if (currentTime - lastStatusLogTime >= 5000) {
        size_t freeHeap = ESP.getFreeHeap();
        String statusMsg = "Status: Free mem: " + String(freeHeap) + " bytes, ";
        statusMsg += "A2DP connected: " + String(bluetoothController && bluetoothController->isA2dpConnected() ? "true" : "false") + ", ";
        statusMsg += "Retrying: " + String(bluetoothController && bluetoothController->isRetryingConnection() ? "true" : "false") + ", ";
        statusMsg += "Audio playing: " + String(audioPlayer && audioPlayer->isAudioPlaying() ? "true" : "false") + ", ";
        statusMsg += "Init queued: " + String(initializationQueued ? "true" : "false") + ", ";
        statusMsg += "Init played: " + String(initializationPlayed ? "true" : "false") + ", ";
        if (wifiManager) {
            statusMsg += "🛜 WiFi: " + String(wifiManager->isConnected() ? "connected" : "disconnected");
            if (wifiManager->isConnected()) {
                statusMsg += " (" + wifiManager->getIPAddress() + ")";
            }
        } else {
            statusMsg += "🛜 WiFi: disabled";
        }
        LOG_INFO(TAG, "%s", statusMsg.c_str());
        lastStatusLogTime = currentTime;
    }

    switch (currentState) {
        case DeathState::IDLE:
            break;

        case DeathState::PLAY_WELCOME:
            if (!audioPlayer->isAudioPlaying()) {
                currentState = DeathState::COOLDOWN;
                stateStartTime = currentTime;
            }
            break;

        case DeathState::FORTUNE_FLOW:
            handleFortuneFlow(currentTime);
            break;

        case DeathState::COOLDOWN:
            if (currentTime - stateStartTime >= 12000) {
                currentState = DeathState::IDLE;
            }
            break;
    }

    delay(1);
}

void handleUARTCommand(UARTCommand cmd) {
    unsigned long currentTime = millis();
    if (currentState != DeathState::IDLE) {
        LOG_WARN(STATE_TAG, "Ignoring command - system busy");
        return;
    }
    if (currentTime - lastTriggerTime < TRIGGER_DEBOUNCE_MS) {
        return;
    }
    switch (cmd) {
        case UARTCommand::TRIGGER_FAR:
            startWelcomeSequence();
            break;
        case UARTCommand::TRIGGER_NEAR:
            startFortuneFlow();
            break;
        default:
            break;
    }
    lastTriggerTime = currentTime;
}

void startWelcomeSequence() {
    LOG_INFO(FLOW_TAG, "Starting welcome sequence");
    currentState = DeathState::PLAY_WELCOME;
    stateStartTime = millis();
    String welcomeFile = "/audio/welcome/welcome_01.wav";
    audioPlayer->playNext(welcomeFile);
}

void startFortuneFlow() {
    LOG_INFO(FLOW_TAG, "Starting fortune flow");
    currentState = DeathState::FORTUNE_FLOW;
    stateStartTime = millis();
    String promptFile = "/audio/fortune/fortune_01.wav";
    audioPlayer->playNext(promptFile);
    servoController.setPosition(80);
    mouthOpen = true;
    lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
}

void handleFortuneFlow(unsigned long currentTime) {
    if (fingerSensor && fingerSensor->isFingerDetected()) {
        if (!fingerDetected) {
            fingerDetected = true;
            fingerDetectionStart = currentTime;
            LOG_INFO(FLOW_TAG, "Finger detected!");
        }
        if (currentTime - fingerDetectionStart >= 120) {
            if (snapDelayMs == 0) {
                snapDelayMs = random(1000, 3001);
                snapDelayStart = currentTime;
                LOG_INFO(FLOW_TAG, "Starting snap delay: %dms", snapDelayMs);
            }
        }
    } else {
        fingerDetected = false;
    }

    if (snapDelayMs > 0 && currentTime - snapDelayStart >= (unsigned long)snapDelayMs) {
        LOG_INFO(FLOW_TAG, "Snap delay elapsed, triggering fortune flow action");
        snapDelayMs = 0;
        // TODO: implement snap action
    }
}
