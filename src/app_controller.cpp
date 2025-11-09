#include "app_controller.h"

#ifdef ARDUINO

#include <algorithm>
#include <cstdarg>
#include <utility>
#include <vector>

#include <WiFi.h>
#include "BluetoothA2DPSource.h"
#include "SD_MMC.h"
#include "audio_directory_selector.h"
#include "audio_player.h"
#include "bluetooth_controller.h"
#include "cli_command_router.h"
#include "cli_service.h"
#include "config_manager.h"
#include "death_controller_adapters.h"
#include "finger_sensor.h"
#include "fortune_generator.h"
#include "infra/log_sink.h"
#include "light_controller.h"
#include "logging_manager.h"
#include "ota_manager.h"
#include "remote_debug_manager.h"
#include "servo_controller.h"
#include "skit_selector.h"
#include "skull_audio_animator.h"
#include "thermal_printer.h"
#include "uart_controller.h"
#include "wifi_manager.h"

namespace {

constexpr char TAG[] = "Main";
constexpr char WIFI_TAG[] = "WiFi";
constexpr char OTA_TAG[] = "OTA";
constexpr char DEBUG_TAG[] = "RemoteDebug";
constexpr char STATE_TAG[] = "State";
constexpr char AUDIO_TAG[] = "Audio";
constexpr char BT_TAG[] = "Bluetooth";
constexpr char FLOW_TAG[] = "FortuneFlow";
constexpr char LED_TAG[] = "LED";

constexpr const char* AUDIO_WELCOME_DIR = "/audio/welcome";
constexpr const char* AUDIO_FINGER_PROMPT_DIR = "/audio/finger_prompt";
constexpr const char* AUDIO_FINGER_SNAP_DIR = "/audio/finger_snap";
constexpr const char* AUDIO_NO_FINGER_DIR = "/audio/no_finger";
constexpr const char* AUDIO_FORTUNE_PREAMBLE_DIR = "/audio/fortune_preamble";
constexpr const char* AUDIO_GOODBYE_DIR = "/audio/goodbye";
constexpr const char* AUDIO_FORTUNE_TEMPLATES_DIR = "/audio/fortune_templates";
constexpr const char* AUDIO_FORTUNE_TOLD_DIR = "/audio/fortune_told";

constexpr unsigned long BREATHING_INTERVAL = 7000;   // ms
constexpr int BREATHING_JAW_ANGLE = 30;              // degrees
constexpr int BREATHING_MOVEMENT_DURATION = 2000;    // ms
constexpr int SERVO_POSITION_MARGIN_DEGREES = 3;

constexpr const char* DEFAULT_FORTUNE_JSON = "/printer/fortunes_littlekid.json";
constexpr const char* DEFAULT_PRINTER_LOGO = "/printer/logo_384w.bmp";
constexpr const char* DEFAULT_INITIALIZATION_AUDIO = "/audio/initialized.wav";

constexpr unsigned INIT_SERIAL_DELAY_MS = 500;
constexpr int MAX_SD_RETRIES = 5;
constexpr int MAX_CONFIG_RETRIES = 5;

int32_t IRAM_ATTR provideAudioFramesThunk(void* context, Frame* frame, int32_t frameCount) {
    auto* player = static_cast<AudioPlayer*>(context);
    if (!player) {
        return 0;
    }
    return player->provideAudioFrames(frame, frameCount);
}

}  // namespace

AppController* AppController::s_instance = nullptr;

AppController::ModuleOptions AppController::ModuleOptions::DefaultsFromBuildFlags() {
    ModuleOptions opts;
    opts.normalize();
    return opts;
}

void AppController::ModuleOptions::normalize() {
    if (!enableConnectivity) {
        enableWifi = false;
        enableOta = false;
        enableRemoteDebug = false;
    }
    if (!enablePrinter) {
        // Nothing additional yet.
    }
}

AppController::SerialPrinter::SerialPrinter(Stream& stream) : m_stream(stream) {}

void AppController::SerialPrinter::print(const String& value) {
    if (&m_stream) {
        m_stream.print(value);
    }
}

void AppController::SerialPrinter::println(const String& value) {
    if (&m_stream) {
        m_stream.println(value);
    }
}

void AppController::SerialPrinter::println() {
    if (&m_stream) {
        m_stream.println();
    }
}

void AppController::SerialPrinter::printf(const char* fmt, ...) {
    if (!fmt || !&m_stream) {
        return;
    }
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    m_stream.print(buffer);
}

AppController::AppController(const HardwarePins& pins,
                             infra::ITimeProvider& timeProvider,
                             infra::IRandomSource& randomSource,
                             infra::ILogSink* logSink,
                             ModuleOptions options,
                             ModuleProviders providers)
    : m_pins(pins),
      m_options(std::move(options)),
      m_providers(std::move(providers)),
      m_timeProvider(timeProvider),
      m_randomSource(randomSource),
      m_logSink(logSink),
      m_lightController(std::make_unique<LightController>(pins.eyeLed, pins.mouthLed)),
      m_servoController(std::make_unique<ServoController>()),
      m_fingerSensor(std::make_unique<FingerSensor>(pins.fingerSensor)) {
    m_options.normalize();
    s_instance = this;

    m_audioDirectorySelector = m_providers.audioSelector;
    m_skitSelector = m_providers.skitSelector;
    m_audioPlayer = m_providers.audioPlayer;
    m_thermalPrinter = m_providers.printer;
    m_wifiManager = m_providers.wifi;
    m_otaManager = m_providers.ota;
    m_remoteDebugManager = m_providers.remoteDebug;
    m_bluetoothController = m_providers.bluetooth;
    m_cliService = m_providers.cliService;
    m_cliRouter = m_providers.cliRouter;

    if (!m_bluetoothController) {
        m_bluetoothControllerOwned = nullptr;
    }
    if (!m_wifiManager) {
        m_wifiManagerOwned = nullptr;
    }
    if (!m_otaManager) {
        m_otaManagerOwned = nullptr;
    }
    if (!m_remoteDebugManager) {
        m_remoteDebugManagerOwned = nullptr;
    }

    m_uartControllerOwned = std::make_unique<UARTController>(m_pins.uartMatterRx, m_pins.uartMatterTx);
    m_uartController = m_uartControllerOwned.get();
}

AppController::~AppController() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool AppController::setup() {
    if (m_initialized) {
        return true;
    }

    Serial.begin(115200);
    delay(INIT_SERIAL_DELAY_MS);

    setupLogging();
    LOG_INFO(TAG, "💀 Death starting…");

    if (!m_lightController) {
        m_lightController = std::make_unique<LightController>(m_pins.eyeLed, m_pins.mouthLed);
    }
    if (!m_servoController) {
        m_servoController = std::make_unique<ServoController>();
    }
    if (!m_fingerSensor) {
        m_fingerSensor = std::make_unique<FingerSensor>(m_pins.fingerSensor);
    }

    m_lightController->begin();
    m_lightController->blinkLights(3);

    mountSdCard();
    loadConfiguration();
    initializeServo();
    initializeAudio();
    initializePrinter();
    initializeFingerSensor();
    initializeBluetooth();
    initializeUart();
    initializeDeathController();
    initializeSkitSystems();
    validateAudioDirectories();
    initializeConnectivity();
    initializeCli();
    queueInitializationAudio();

    LOG_INFO(TAG, "🎉 Death initialized successfully");
    m_initialized = true;
    return true;
}

void AppController::loop() {
    if (!m_initialized) {
        return;
    }

    const unsigned long now = millis();

    if (m_audioPlayer) {
        m_audioPlayer->update();
    }
    if (m_bluetoothController) {
        m_bluetoothController->update();
    }
    if (m_fingerSensor) {
        m_fingerSensor->update();
    }
    if (m_thermalPrinter) {
        m_thermalPrinter->update();
        updatePrinterFaultIndicator();
    }
    if (m_lightController) {
        m_lightController->update();
    }

    updateConnectivity();

    if (m_uartController) {
        m_uartController->update();
        UARTCommand lastCommand = m_uartController->getLastCommand();
        if (lastCommand != UARTCommand::NONE) {
            handleUartCommand(lastCommand);
            m_uartController->clearLastCommand();
        }
        if (now - m_lastHandshakeReport > 30000UL) {
            const bool bootComplete = m_uartController->isBootHandshakeComplete();
            const bool fabricComplete = m_uartController->isFabricHandshakeComplete();
            LOG_INFO(STATE_TAG,
                     "UART Handshake Status - Boot: %s, Fabric: %s",
                     bootComplete ? "OK" : "PENDING",
                     fabricComplete ? "OK" : "PENDING");
            m_lastHandshakeReport = now;
        }
    }

    if (m_deathController) {
        DeathController::FingerReadout readout{};
        if (m_fingerSensor) {
            readout.detected = m_fingerSensor->isFingerDetected();
            readout.stable = m_fingerSensor->hasStableTouch();
            readout.normalizedDelta = m_fingerSensor->getNormalizedDelta();
            readout.thresholdRatio = m_fingerSensor->getThresholdRatio();
        }
        m_deathController->update(now, readout);
        processControllerActions(m_deathController->pendingActions());
        m_deathController->clearActions();

        const bool fingerStreaming = m_fingerSensor && m_fingerSensor->isStreamEnabled();
        const bool controllerIdle = m_deathController->state() == DeathController::State::Idle;
        if (!fingerStreaming && m_audioPlayer && controllerIdle &&
            now - m_lastJawMovementTime >= BREATHING_INTERVAL &&
            !m_audioPlayer->isAudioPlaying()) {
            breathingJawMovement();
            m_lastJawMovementTime = now;
        }
    }

    if (m_cliService) {
        m_cliService->poll();
    }
}

void AppController::enqueueCliCommand(const String& command) {
    if (m_cliService) {
        m_cliService->enqueueCommand(command);
    }
}

void AppController::setupLogging() {
    LoggingManager::instance().begin(&Serial);
    class LoggingManagerSink : public infra::ILogSink {
    public:
        void log(infra::LogLevel level, const char* tag, const char* message) override {
            LoggingManager::instance().log(static_cast<LogLevel>(level),
                                           tag ? tag : TAG,
                                           "%s",
                                           message ? message : "");
        }
    };
    static LoggingManagerSink loggingSink;
    infra::setLogSink(&loggingSink);

    if (m_logSink) {
        infra::setLogSink(m_logSink);
    }
}

void AppController::mountSdCard() {
    m_sdCardMounted = false;
    int retries = 0;
    while (!m_sdCardManager.begin() && retries < MAX_SD_RETRIES) {
        LOG_WARN(TAG, "⚠️ SD card mount failed! Retrying… (%d/%d)", retries + 1, MAX_SD_RETRIES);
        m_lightController->blinkEyes(3);
        delay(500);
        ++retries;
    }

    if (retries < MAX_SD_RETRIES) {
        m_sdCardMounted = true;
        LOG_INFO(TAG, "SD card mounted successfully");
        m_sdCardContent = m_sdCardManager.loadContent();
    } else {
        LOG_WARN(TAG, "⚠️ SD card mount failed after %d retries - using safe defaults", MAX_SD_RETRIES);
    }
}

void AppController::loadConfiguration() {
    ConfigManager& config = ConfigManager::getInstance();
    int retries = 0;
    while (!config.loadConfig() && retries < MAX_CONFIG_RETRIES) {
        LOG_WARN(TAG, "⚠️ Failed to load config. Retrying… (%d/%d)", retries + 1, MAX_CONFIG_RETRIES);
        m_lightController->blinkEyes(5);
        delay(500);
        ++retries;
    }

    if (retries < MAX_CONFIG_RETRIES) {
        m_configLoaded = true;
        LOG_INFO(TAG, "Configuration loaded successfully");
    } else {
        LOG_WARN(TAG, "⚠️ Config failed to load after %d retries", MAX_CONFIG_RETRIES);
        m_configLoaded = false;
    }

    m_fortunesJsonPath = m_configLoaded ? config.getFortunesJson() : String();
    if (m_fortunesJsonPath.isEmpty()) {
        m_fortunesJsonPath = DEFAULT_FORTUNE_JSON;
    }

    m_printerLogoPath = m_configLoaded ? config.getPrinterLogo() : String();
    if (m_printerLogoPath.isEmpty()) {
        m_printerLogoPath = DEFAULT_PRINTER_LOGO;
    }

    m_initializationAudioPath = DEFAULT_INITIALIZATION_AUDIO;

    if (m_configLoaded) {
        m_fingerStableMs = config.getFingerDetectMs();
        m_fingerWaitMs = config.getFingerWaitMs();
        m_snapDelayMinMs = config.getSnapDelayMinMs();
        m_snapDelayMaxMs = config.getSnapDelayMaxMs();
        m_cooldownMs = config.getCooldownMs();
        if (m_snapDelayMinMs > m_snapDelayMaxMs) {
            std::swap(m_snapDelayMinMs, m_snapDelayMaxMs);
        }
        m_lightController->configureMouthLED(config.getMouthLedBright(),
                                             config.getMouthLedPulseMin(),
                                             config.getMouthLedPulseMax(),
                                             config.getMouthLedPulsePeriodMs());
        LOG_INFO(FLOW_TAG,
                 "Timer config — fingerStable=%lums fingerWait=%lums snapDelay=%lu-%lums cooldown=%lums",
                 m_fingerStableMs,
                 m_fingerWaitMs,
                 m_snapDelayMinMs,
                 m_snapDelayMaxMs,
                 m_cooldownMs);
    } else {
        LOG_INFO(FLOW_TAG,
                 "Timer defaults — fingerStable=%lums fingerWait=%lums snapDelay=%lu-%lums cooldown=%lums",
                 m_fingerStableMs,
                 m_fingerWaitMs,
                 m_snapDelayMinMs,
                 m_snapDelayMaxMs,
                 m_cooldownMs);
    }

    m_fortuneCandidates = gatherFortuneCandidates(m_fortunesJsonPath);
}

void AppController::initializeServo() {
    ConfigManager& config = ConfigManager::getInstance();
    const int servoPin = m_pins.servo;
    if (!m_servoController) {
        return;
    }
    if (m_configLoaded) {
        const int minUs = config.getServoUSMin();
        const int maxUs = config.getServoUSMax();
        LOG_INFO(TAG, "Initializing servo with config values: %d-%d µs", minUs, maxUs);
        m_servoController->initialize(servoPin, 0, 80, minUs, maxUs);
    } else {
        constexpr int SAFE_MIN_US = 1400;
        constexpr int SAFE_MAX_US = 1600;
        LOG_INFO(TAG, "Initializing servo with safe defaults: %d-%d µs", SAFE_MIN_US, SAFE_MAX_US);
        m_servoController->initialize(servoPin, 0, 80, SAFE_MIN_US, SAFE_MAX_US);
    }
}

void AppController::initializeAudio() {
    if (!m_audioDirectorySelector) {
        m_audioDirectorySelectorOwned = std::make_unique<AudioDirectorySelector>();
        m_audioDirectorySelector = m_audioDirectorySelectorOwned.get();
    }

    if (!m_audioPlayer) {
        m_audioPlayerOwned = std::make_unique<AudioPlayer>(m_sdCardManager);
        m_audioPlayer = m_audioPlayerOwned.get();
    }

    if (!m_audioPlannerAdapter) {
        m_audioPlannerAdapter = std::make_unique<AudioPlannerAdapter>(*m_audioDirectorySelector);
    }
    m_audioPlannerAdapter->setAudioPlayer(m_audioPlayer);

    m_audioPlayer->setPlaybackStartCallback(&AppController::audioStartThunk);
    m_audioPlayer->setPlaybackEndCallback(&AppController::audioEndThunk);
    m_audioPlayer->setAudioFramesProvidedCallback(&AppController::audioFramesThunk);
}

void AppController::initializePrinter() {
    if (!m_options.enablePrinter) {
        m_thermalPrinter = nullptr;
        return;
    }

    ConfigManager& config = ConfigManager::getInstance();
    if (!m_thermalPrinter) {
        m_thermalPrinterOwned = std::make_unique<ThermalPrinter>(Serial2,
                                                                 m_pins.printerTx,
                                                                 m_pins.printerRx,
                                                                 config.getPrinterBaud());
        m_thermalPrinter = m_thermalPrinterOwned.get();
    }

    if (!m_printerStatusAdapter && m_thermalPrinter) {
        m_printerStatusAdapter = std::make_unique<PrinterStatusAdapter>(*m_thermalPrinter);
    }

    if (m_thermalPrinter) {
        m_thermalPrinter->setLogoPath(m_printerLogoPath);
        m_thermalPrinter->begin();
    } else {
        LOG_WARN(FLOW_TAG, "Thermal printer unavailable");
    }
}

void AppController::initializeFingerSensor() {
    if (!m_fingerSensor) {
        return;
    }
    ConfigManager& config = ConfigManager::getInstance();
    m_fingerSensor->setTouchCycles(config.getFingerCyclesInit(), config.getFingerCyclesMeasure());
    m_fingerSensor->setFilterAlpha(config.getFingerFilterAlpha());
    m_fingerSensor->setBaselineDrift(config.getFingerBaselineDrift());
    m_fingerSensor->setMultisampleCount(config.getFingerMultisample());
    m_fingerSensor->setSensitivity(config.getCapThreshold());
    m_fingerSensor->begin();
    m_fingerSensor->setStableDurationMs(m_fingerStableMs);
    m_fingerSensor->setStreamIntervalMs(500);

    if (!m_manualCalibrationAdapter) {
        m_manualCalibrationAdapter =
            std::make_unique<ManualCalibrationAdapter>(*m_lightController,
                                                       *m_fingerSensor,
                                                       ConfigManager::getInstance());
    }
}

void AppController::initializeBluetooth() {
    ConfigManager& config = ConfigManager::getInstance();
    bool bluetoothEnabledConfig = config.isBluetoothEnabled();
#ifdef DISABLE_BLUETOOTH
    bluetoothEnabledConfig = false;
#endif
    if (!m_options.enableBluetooth) {
        bluetoothEnabledConfig = false;
    }

    if (!bluetoothEnabledConfig) {
        m_bluetoothController = nullptr;
        m_bluetoothControllerOwned.reset();
        LOG_WARN(BT_TAG, "Bluetooth disabled (config or build flag)");
        return;
    }

    if (!m_bluetoothController) {
        m_bluetoothControllerOwned = std::make_unique<BluetoothController>();
        m_bluetoothController = m_bluetoothControllerOwned.get();
    }

    if (!m_audioPlayer) {
        LOG_WARN(BT_TAG, "Bluetooth controller enabled but audio player missing");
        return;
    }

    m_bluetoothController->initializeA2DP(config.getBluetoothSpeakerName(),
                                          provideAudioFramesThunk,
                                          m_audioPlayer);
    m_bluetoothController->setConnectionStateChangeCallback([this](int state) {
        if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            const bool isPlaying = m_audioPlayer && m_audioPlayer->isAudioPlaying();
            const bool hasQueue = m_audioPlayer && m_audioPlayer->hasQueuedAudio();
            LOG_INFO(BT_TAG,
                     "🔗 Bluetooth speaker connected. initPlayed=%s, isAudioPlaying=%s, hasQueued=%s",
                     m_initializationPlayed ? "true" : "false",
                     isPlaying ? "true" : "false",
                     hasQueue ? "true" : "false");
            if (!m_initializationPlayed && m_audioPlayer && !m_initializationQueued) {
                LOG_INFO(BT_TAG, "🎬 Priming initialization audio after Bluetooth connect");
                m_audioPlayer->playNext(m_initializationAudioPath);
                m_initializationQueued = true;
            }
        } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            LOG_WARN(BT_TAG, "🔌 Bluetooth speaker disconnected");
        }
    });
    m_bluetoothController->set_volume(config.getSpeakerVolume());
    m_bluetoothController->startConnectionRetry();
}

void AppController::initializeUart() {
    if (m_uartController) {
        m_uartController->begin();
    }
}

void AppController::initializeDeathController() {
    if (!m_audioPlannerAdapter || !m_audioPlayer) {
        LOG_WARN(FLOW_TAG, "Audio planner unavailable; skipping DeathController");
        return;
    }

    if (!m_fortuneServiceAdapter) {
        m_fortuneServiceAdapter = std::make_unique<FortuneServiceAdapter>(m_fortuneGenerator);
    }
    if (!m_manualCalibrationAdapter && m_fingerSensor) {
        m_manualCalibrationAdapter =
            std::make_unique<ManualCalibrationAdapter>(*m_lightController,
                                                       *m_fingerSensor,
                                                       ConfigManager::getInstance());
    }

    DeathController::Dependencies deps{};
    deps.time = &m_timeProvider;
    deps.random = &m_randomSource;
    deps.log = infra::getLogSink();
    deps.audioPlanner = m_audioPlannerAdapter.get();
    deps.fortuneService = m_fortuneServiceAdapter.get();
    deps.printerStatus = m_printerStatusAdapter ? m_printerStatusAdapter.get() : nullptr;
    deps.manualCalibDriver = m_manualCalibrationAdapter ? m_manualCalibrationAdapter.get() : nullptr;

    m_deathController = std::make_unique<DeathController>(deps);

    DeathController::ConfigSnapshot snapshot{};
    snapshot.fingerStableMs = m_fingerStableMs;
    snapshot.fingerWaitMs = m_fingerWaitMs;
    snapshot.snapDelayMinMs = m_snapDelayMinMs;
    snapshot.snapDelayMaxMs = m_snapDelayMaxMs;
    snapshot.cooldownMs = m_cooldownMs;
    snapshot.welcomeDir = AUDIO_WELCOME_DIR;
    snapshot.fingerPromptDir = AUDIO_FINGER_PROMPT_DIR;
    snapshot.fingerSnapDir = AUDIO_FINGER_SNAP_DIR;
    snapshot.noFingerDir = AUDIO_NO_FINGER_DIR;
    snapshot.fortunePreambleDir = AUDIO_FORTUNE_PREAMBLE_DIR;
    snapshot.fortuneDoneDir = AUDIO_FORTUNE_TOLD_DIR;
    if (!m_fortuneCandidates.empty()) {
        snapshot.fortuneFlowDir = m_fortuneCandidates.front();
    } else {
        snapshot.fortuneFlowDir = m_fortunesJsonPath.c_str();
    }
    snapshot.fortuneCandidates = m_fortuneCandidates;

    m_deathController->initialize(snapshot);
    m_deathController->clearActions();
}

void AppController::initializeSkitSystems() {
    const int servoMinDegrees = 0;
    const int servoMaxDegrees = 80;
    const bool isPrimary = true;

    if (!m_skullAudioAnimator) {
        m_skullAudioAnimator = std::make_unique<SkullAudioAnimator>(isPrimary,
                                                                    *m_servoController,
                                                                    *m_lightController,
                                                                    m_sdCardContent.skits,
                                                                    m_sdCardManager,
                                                                    servoMinDegrees,
                                                                    servoMaxDegrees);
        m_skullAudioAnimator->setSpeakingStateCallback([this](bool speaking) {
            if (!m_lightController) {
                return;
            }
            m_lightController->setEyeBrightness(speaking ? LightController::BRIGHTNESS_MAX
                                                         : LightController::BRIGHTNESS_DIM);
        });
    }

    if (!m_skitSelector) {
        m_skitSelectorOwned = std::make_unique<SkitSelector>(m_sdCardContent.skits);
        m_skitSelector = m_skitSelectorOwned.get();
    }

    testSkitSelection();
    testCategorySelection(AUDIO_WELCOME_DIR, "welcome skit");
    testCategorySelection(AUDIO_FORTUNE_PREAMBLE_DIR, "fortune preamble");
}

void AppController::initializeConnectivity() {
    ConfigManager& config = ConfigManager::getInstance();

    if (m_options.enableConnectivity && !m_remoteDebugManager) {
        m_remoteDebugManagerOwned = std::make_unique<RemoteDebugManager>();
        m_remoteDebugManager = m_remoteDebugManagerOwned.get();
    }

    if (m_remoteDebugManager) {
        m_remoteDebugManager->setBluetoothController(m_bluetoothController);
    }

    auto pauseRemoteDebugForOta = [this]() {
        if (!m_remoteDebugManager) {
            return;
        }
        m_remoteDebugStreamingWasEnabled = m_remoteDebugManager->isAutoStreaming();
        if (m_remoteDebugStreamingWasEnabled) {
            m_remoteDebugManager->setAutoStreaming(false);
            m_remoteDebugManager->println("🛜 RemoteDebug: auto streaming paused during OTA");
        }
    };

    auto restoreRemoteDebugAfterOta = [this](const char* enabledMsg, const char* disabledMsg) {
        if (!m_remoteDebugManager) {
            return;
        }
        if (m_remoteDebugStreamingWasEnabled) {
            m_remoteDebugManager->setAutoStreaming(true);
            m_remoteDebugManager->println(enabledMsg);
        } else {
            m_remoteDebugManager->println(disabledMsg);
        }
        m_remoteDebugStreamingWasEnabled = m_remoteDebugManager->isAutoStreaming();
    };

    if (m_options.enableConnectivity && m_options.enableOta) {
        if (!m_otaManager) {
            m_otaManagerOwned = std::make_unique<OTAManager>();
            m_otaManager = m_otaManagerOwned.get();
        }
        if (m_otaManager) {
            m_otaManager->setOnStartCallback(pauseRemoteDebugForOta);
            m_otaManager->setOnEndCallback([restoreRemoteDebugAfterOta]() {
                restoreRemoteDebugAfterOta("🛜 RemoteDebug: auto streaming resumed after OTA",
                                           "🛜 RemoteDebug: auto streaming left disabled after OTA");
            });
            m_otaManager->setOnErrorCallback([restoreRemoteDebugAfterOta](ota_error_t) {
                restoreRemoteDebugAfterOta("🛜 RemoteDebug: auto streaming resumed after OTA abort",
                                           "🛜 RemoteDebug: auto streaming left disabled after OTA abort");
            });
        }
    } else {
        m_otaManager = nullptr;
        m_otaManagerOwned.reset();
    }

    if (m_options.enableConnectivity && m_options.enableWifi) {
        if (!m_wifiManager) {
            m_wifiManagerOwned = std::make_unique<WiFiManager>();
            m_wifiManager = m_wifiManagerOwned.get();
        }

        const String wifiSSID = config.getWiFiSSID();
        const String wifiPassword = config.getWiFiPassword();
        const String otaHostname = config.getOTAHostname();
        const String otaPassword = config.getOTAPassword();

        LOG_INFO(WIFI_TAG, "🛜 Checking Wi-Fi configuration…");
        LOG_INFO(WIFI_TAG, "   SSID: %s", wifiSSID.length() > 0 ? wifiSSID.c_str() : "[NOT SET]");
        LOG_INFO(WIFI_TAG, "   Password: %s", wifiPassword.length() > 0 ? "[SET]" : "[NOT SET]");
        LOG_INFO(WIFI_TAG, "   OTA Hostname: %s", otaHostname.c_str());
        LOG_INFO(WIFI_TAG, "   OTA Password: %s", otaPassword.length() > 0 ? "[SET]" : "[NOT SET]");

        if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
            LOG_INFO(WIFI_TAG, "Initializing Wi-Fi manager");
            m_wifiManager->setHostname(otaHostname);
            m_wifiManager->setConnectionCallback([this, wifiSSID, otaHostname, otaPassword](bool connected) {
                if (connected) {
                    LOG_INFO(WIFI_TAG,
                             "🛜 Connected! SSID: %s, IP: %s",
                             wifiSSID.c_str(),
                             m_wifiManager->getIPAddress().c_str());
                    if (m_remoteDebugManager && m_remoteDebugManager->begin(23)) {
                        LOG_INFO(DEBUG_TAG,
                                 "🛜 Telnet server started on port 23 (telnet %s 23)",
                                 m_wifiManager->getIPAddress().c_str());
                    }
                    if (m_otaManager && !m_otaManager->isEnabled()) {
                        if (m_otaManager->begin(otaHostname, otaPassword)) {
                            LOG_INFO(OTA_TAG, "🔄 OTA manager started (port 3232)");
                            if (otaPassword.length() > 0) {
                                LOG_INFO(OTA_TAG, "🔐 OTA password protection enabled");
                            }
                        } else if (m_otaManager->disabledForMissingPassword()) {
                            LOG_ERROR(OTA_TAG, "OTA password missing; OTA disabled");
                        } else {
                            LOG_ERROR(OTA_TAG, "❌ OTA manager failed to start");
                        }
                    }
                } else {
                    LOG_WARN(WIFI_TAG, "⚠️ Wi-Fi connection failed");
                }
            });
            m_wifiManager->setDisconnectionCallback([]() {
                LOG_WARN(WIFI_TAG, "⚠️ Wi-Fi disconnected");
            });
            if (m_wifiManager->begin(wifiSSID, wifiPassword)) {
                LOG_INFO(WIFI_TAG, "Wi-Fi manager started, attempting connection…");
            } else {
                LOG_ERROR(WIFI_TAG, "❌ Wi-Fi manager failed to start");
            }
        } else {
            LOG_WARN(WIFI_TAG, "⚠️ Wi-Fi credentials incomplete or missing; wireless features disabled");
        }
    } else {
        m_wifiManager = nullptr;
        m_wifiManagerOwned.reset();
    }
}

void AppController::initializeCli() {
    if (!m_options.enableCli) {
        m_cliService = nullptr;
        m_cliRouter = nullptr;
        return;
    }

    if (!m_cliPrinter) {
        m_cliPrinter = std::make_unique<SerialPrinter>(Serial);
    }

    configureCliRouter();

    if (!m_cliService) {
        if (m_providers.cliService) {
            m_cliService = m_providers.cliService;
        } else {
            auto handler = [this](const String& cmd) {
                if (m_cliRouter) {
                    m_cliRouter->handleCommand(cmd);
                }
            };
            m_cliServiceOwned = std::make_unique<CliService>(Serial, handler);
            m_cliService = m_cliServiceOwned.get();
        }
    }
}

void AppController::queueInitializationAudio() {
    if (!m_audioPlayer) {
        return;
    }
    if (m_sdCardManager.fileExists(m_initializationAudioPath.c_str())) {
        m_audioPlayer->playNext(m_initializationAudioPath);
        LOG_INFO(AUDIO_TAG,
                 "🎵 Queued initialization audio: %s",
                 m_initializationAudioPath.c_str());
        m_initializationQueued = true;
        m_initializationPlayed = false;
    } else {
        LOG_WARN(AUDIO_TAG,
                 "⚠️ Initialization audio missing: %s",
                 m_initializationAudioPath.c_str());
    }
}

void AppController::processControllerActions(const DeathController::ControllerActions& actions) {
    if (!m_deathController) {
        return;
    }
    if (!actions.audioToQueue.empty() && m_audioPlayer) {
        for (const std::string& clip : actions.audioToQueue) {
            if (clip.empty()) {
                continue;
            }
            LOG_INFO(FLOW_TAG, "Controller queuing audio: %s", clip.c_str());
            m_audioPlayer->playNext(String(clip.c_str()));
        }
    }
    if (!actions.fortuneText.empty()) {
        printFortuneToSerial(String(actions.fortuneText.c_str()));
    }

    if (actions.requestMouthOpen || actions.requestMouthClose) {
        const int servoOpen = getServoOpenPosition();
        const int servoClosed = getServoClosedPosition();
        if (actions.requestMouthOpen && m_servoController) {
            m_servoController->setPosition(servoOpen);
            m_mouthOpen = true;
        } else if (actions.requestMouthClose && m_servoController) {
            m_servoController->setPosition(servoClosed);
            m_mouthOpen = false;
        }
    }

    if (actions.requestMouthPulseEnable && m_lightController) {
        m_lightController->setMouthPulse();
        m_mouthPulseActive = true;
    }
    if (actions.requestMouthPulseDisable && m_lightController) {
        m_lightController->setMouthOff();
        m_mouthPulseActive = false;
    }

    if (actions.requestLedPrompt && m_lightController) {
        m_lightController->setEyeBrightness(LightController::BRIGHTNESS_MAX);
        m_lightController->setMouthBright();
    }
    if (actions.requestLedIdle && m_lightController) {
        m_lightController->setEyeBrightness(LightController::BRIGHTNESS_DIM);
        m_lightController->setMouthOff();
    }
    if (actions.requestLedFingerDetected && m_lightController) {
        m_lightController->setEyeBrightness(LightController::BRIGHTNESS_MAX);
        m_lightController->setMouthBright();
    }

    if (actions.queueFortunePrint) {
        bool success = false;
        if (m_thermalPrinter) {
            success = m_thermalPrinter->queueFortunePrint(String(actions.fortuneText.c_str()));
        }
        if (!success) {
            LOG_WARN(FLOW_TAG, "Controller requested fortune print but printer unavailable or failed");
        }
    }

    if (actions.requestRemoteDebugPause && m_remoteDebugManager) {
        m_remoteDebugManager->setAutoStreaming(false);
    }
    if (actions.requestRemoteDebugResume && m_remoteDebugManager) {
        m_remoteDebugManager->setAutoStreaming(true);
    }
}

void AppController::updatePrinterFaultIndicator() {
    if (!m_thermalPrinter || m_printerFaultLatched || !m_lightController) {
        return;
    }
    if (m_thermalPrinter->hasError()) {
        m_printerFaultLatched = true;
        LOG_WARN(LED_TAG, "Printer fault detected; latching eye fault indicator");
        m_lightController->startEyeBlinkPattern(3,
                                                120,
                                                120,
                                                800,
                                                LightController::BRIGHTNESS_MAX,
                                                LightController::BRIGHTNESS_OFF,
                                                2,
                                                "Printer fault");
    }
}

void AppController::breathingJawMovement() {
    if (!m_audioPlayer || m_audioPlayer->isAudioPlaying() || !m_servoController) {
        return;
    }
    const int closedPosition = getServoClosedPosition();
    int openTarget = closedPosition + BREATHING_JAW_ANGLE;
    openTarget = std::min(openTarget, getServoOpenPosition());
    m_servoController->smoothMove(openTarget, BREATHING_MOVEMENT_DURATION);
    delay(100);
    m_servoController->smoothMove(closedPosition, BREATHING_MOVEMENT_DURATION);
}

void AppController::handleUartCommand(UARTCommand cmd) {
    LOG_INFO(STATE_TAG, "Handling UART command: %s", UARTController::commandToString(cmd));

    if (cmd == UARTCommand::LEGACY_PING || cmd == UARTCommand::LEGACY_SET_MODE) {
        LOG_WARN(STATE_TAG,
                 "Legacy UART command ignored: %s",
                 UARTController::commandToString(cmd));
        return;
    }

    if (cmd == UARTCommand::BOOT_HELLO || cmd == UARTCommand::FABRIC_HELLO) {
        LOG_INFO(STATE_TAG, "Handshake command processed: %s", UARTController::commandToString(cmd));
    }

    if (!m_deathController) {
        LOG_WARN(STATE_TAG, "DeathController not initialized; command ignored");
        return;
    }

    m_deathController->handleUartCommand(cmd);
    processControllerActions(m_deathController->pendingActions());
    m_deathController->clearActions();
}

void AppController::printFortuneToSerial(const String& fortune) {
    Serial.println();
    Serial.println(F("=== FORTUNE ==="));
    if (fortune.length() == 0) {
        Serial.println(F("(empty fortune)"));
    } else {
        constexpr size_t chunkSize = 96;
        size_t offset = 0;
        while (offset < fortune.length()) {
            const size_t end = std::min(offset + chunkSize, static_cast<size_t>(fortune.length()));
            Serial.println(fortune.substring(offset, end));
            offset = end;
        }
    }
    Serial.println(F("================"));
    Serial.println();
}

void AppController::logAudioDirectoryTree(const char* path, int depth) {
    if (!path || depth > 6) {
        return;
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
                const size_t sizeBytes = entry.size();
                if (sizeBytes == 0) {
                    LOG_WARN(AUDIO_TAG,
                             "%*s⚠️  %s (0 bytes)",
                             depth * 2 + 2,
                             "",
                             fullPath.c_str());
                } else {
                    LOG_INFO(AUDIO_TAG,
                             "%*s🎵 %s (%u bytes)",
                             depth * 2 + 2,
                             "",
                             fullPath.c_str(),
                             static_cast<unsigned>(sizeBytes));
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
}

void AppController::validateAudioDirectories() {
    if (!m_sdCardMounted) {
        return;
    }
    struct DirCheck {
        const char* path;
        const char* description;
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

    LOG_INFO(AUDIO_TAG, "Audio directory validation starting...");
    logAudioDirectoryTree("/audio", 0);

    for (const auto& check : checks) {
        const int count = countWavFilesInDirectory(check.path);
        if (count <= 0) {
            if (check.optional) {
                LOG_WARN(AUDIO_TAG,
                         "Audio directory '%s' is empty or missing (%s) — optional.",
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

void AppController::testSkitSelection() {
    if (!m_skitSelector) {
        LOG_WARN(FLOW_TAG, "SkitSelector not available for testing");
        return;
    }
    LOG_INFO(FLOW_TAG, "Testing skit selection (repeat prevention)...");
    ParsedSkit firstSkit = m_skitSelector->selectNextSkit();
    if (firstSkit.audioFile.isEmpty()) {
        LOG_WARN(FLOW_TAG, "No skits available for testing");
        return;
    }
    LOG_INFO(FLOW_TAG, "Test selection 1: %s", firstSkit.audioFile.c_str());
    for (int i = 1; i < 5; ++i) {
        ParsedSkit selectedSkit = m_skitSelector->selectNextSkit();
        if (selectedSkit.audioFile.isEmpty()) {
            LOG_WARN(FLOW_TAG, "No skits available for test selection %d", i + 1);
            break;
        }
        LOG_INFO(FLOW_TAG, "Test selection %d: %s", i + 1, selectedSkit.audioFile.c_str());
        delay(100);
    }
    LOG_INFO(FLOW_TAG, "Skit selection test completed");
}

void AppController::testCategorySelection(const char* directory, const char* label) {
    if (!m_audioDirectorySelector) {
        LOG_WARN(AUDIO_TAG,
                 "Audio selector unavailable; skipping %s category test",
                 label ? label : "(unknown)");
        return;
    }
    const int available = countWavFilesInDirectory(directory);
    if (available <= 0) {
        LOG_WARN(AUDIO_TAG,
                 "Skipping %s selection test — available clips: %d",
                 label ? label : "(unknown)",
                 available);
        m_audioDirectorySelector->resetStats(directory);
        return;
    }
    const int iterations = available > 1 ? 4 : 1;
    String last;
    bool repeatDetected = false;
    for (int i = 0; i < iterations; ++i) {
        String clip = m_audioDirectorySelector->selectClip(directory, label);
        if (clip.isEmpty()) {
            LOG_WARN(AUDIO_TAG,
                     "Selection returned empty for %s on iteration %d",
                     label ? label : "(unknown)",
                     i + 1);
            repeatDetected = true;
            break;
        }
        if (!last.isEmpty() && clip == last && available > 1) {
            LOG_WARN(AUDIO_TAG,
                     "Immediate repeat detected for %s: %s",
                     label ? label : "(unknown)",
                     clip.c_str());
            repeatDetected = true;
            break;
        }
        last = clip;
        delay(25);
    }
    if (!repeatDetected) {
        LOG_INFO(AUDIO_TAG,
                 "Category selector validation passed for %s (clips=%d)",
                 label ? label : "(unknown)",
                 available);
    }
    m_audioDirectorySelector->resetStats(directory);
}

int AppController::countWavFilesInDirectory(const char* directory) {
    File dir = SD_MMC.open(directory);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        return -1;
    }
    int count = 0;
    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            name.trim();
            if (!name.startsWith(".")) {
                const size_t fileSize = entry.size();
                if (fileSize > 0 && (name.endsWith(".wav") || name.endsWith(".WAV"))) {
                    ++count;
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return count;
}

int AppController::computeServoMarginDegrees() const {
    if (!m_servoController) {
        return 0;
    }
    const int minDeg = m_servoController->getMinDegrees();
    const int maxDeg = m_servoController->getMaxDegrees();
    const int span = maxDeg - minDeg;
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

int AppController::getServoClosedPosition() const {
    if (!m_servoController) {
        return 0;
    }
    const int minDeg = m_servoController->getMinDegrees();
    const int margin = computeServoMarginDegrees();
    const int maxDeg = m_servoController->getMaxDegrees();
    if (margin <= 0 || minDeg + margin >= maxDeg) {
        return minDeg;
    }
    return minDeg + margin;
}

int AppController::getServoOpenPosition() const {
    if (!m_servoController) {
        return 0;
    }
    const int maxDeg = m_servoController->getMaxDegrees();
    const int minDeg = m_servoController->getMinDegrees();
    const int margin = computeServoMarginDegrees();
    if (margin <= 0 || maxDeg - margin <= minDeg) {
        return maxDeg;
    }
    return maxDeg - margin;
}

std::vector<std::string> AppController::gatherFortuneCandidates(const String& configuredPath) {
    std::vector<std::string> result;
    auto pushUnique = [&](const std::string& path) {
        if (!path.empty() &&
            std::find(result.begin(), result.end(), path) == result.end()) {
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

    for (const String& candidateRaw : candidates) {
        String candidate = sanitizePath(candidateRaw);
        if (candidate.isEmpty()) {
            continue;
        }

        bool handled = false;
        if (m_sdCardMounted) {
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
                    for (const auto& path : files) {
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
            } else if (candidate.endsWith(".json") || candidate.endsWith(".JSON")) {
                const int slashIndex = candidate.lastIndexOf('/');
                const String basename =
                    (slashIndex >= 0) ? candidate.substring(slashIndex + 1) : candidate;
                const String altPath = "/fortunes/" + basename;
                if (m_sdCardManager.fileExists(altPath.c_str())) {
                    pushUnique(altPath.c_str());
                    handled = true;
                }
            }
        }

        if (!handled) {
            pushUnique(candidate.c_str());
        }
    }

    return result;
}

void AppController::configureCliRouter() {
    if (!m_options.enableCli) {
        return;
    }
    if (m_cliRouter) {
        return;
    }

    CliCommandRouter::Dependencies deps{};
    deps.config = &ConfigManager::getInstance();
    deps.printer = m_cliPrinter.get();
    deps.fingerSensor = m_fingerSensor.get();
    deps.fingerStableDurationMs = &m_fingerStableMs;
    deps.fingerStatusPrinter = static_cast<Print*>(&Serial);
    deps.servoController = m_servoController.get();
    deps.servoPin = const_cast<int*>(&m_pins.servo);
    deps.thermalPrinter = m_thermalPrinter;
    deps.configPrinter = [this]() {
        Serial.println();
        Serial.println(F("=== CONFIGURATION SETTINGS ==="));
        ConfigManager::getInstance().printConfig();
        Serial.println();
    };
    deps.sdInfoPrinter = [this](CliCommandRouter::IPrinter& printer) {
        printer.println("\n=== SD CARD CONTENT ===");
        printer.printf("Skits loaded:     %u\n",
                       static_cast<unsigned>(m_sdCardContent.skits.size()));
        printer.printf("Audio files:      %u\n",
                       static_cast<unsigned>(m_sdCardContent.audioFiles.size()));
        if (!m_sdCardContent.skits.empty()) {
            printer.println("\nSkits:");
            const size_t limit = std::min<size_t>(10, m_sdCardContent.skits.size());
            for (size_t i = 0; i < limit; ++i) {
                printer.printf("  %u. %s\n",
                               static_cast<unsigned>(i + 1),
                               m_sdCardContent.skits[i].audioFile.c_str());
            }
            if (m_sdCardContent.skits.size() > limit) {
                printer.printf("  ... and %u more\n",
                               static_cast<unsigned>(m_sdCardContent.skits.size() - limit));
            }
        }
        printer.println();
    };

    m_cliRouterOwned = std::make_unique<CliCommandRouter>(deps);
    m_cliRouter = m_cliRouterOwned.get();
}

void AppController::updateConnectivity() {
    if (m_wifiManager) {
        m_wifiManager->update();
    }
    if (m_otaManager && m_otaManager->isEnabled()) {
        m_otaManager->update();
    }
    if (m_remoteDebugManager) {
        m_remoteDebugManager->update();
    }
}

void AppController::audioStartThunk(const String& filePath) {
    if (s_instance) {
        s_instance->onAudioStart(filePath);
    }
}

void AppController::audioEndThunk(const String& filePath) {
    if (s_instance) {
        s_instance->onAudioEnd(filePath);
    }
}

void AppController::audioFramesThunk(const String& filePath, const Frame* frames, int32_t frameCount) {
    if (s_instance) {
        s_instance->onAudioFrames(filePath, frames, frameCount);
    }
}

void AppController::onAudioStart(const String& filePath) {
    LOG_INFO(AUDIO_TAG, "▶️ Audio playback started: %s", filePath.c_str());
    if (m_deathController) {
        m_deathController->handleAudioStarted(std::string(filePath.c_str()));
        processControllerActions(m_deathController->pendingActions());
        m_deathController->clearActions();
    }
    if (filePath.equals(m_initializationAudioPath)) {
        m_initializationQueued = false;
    }
}

void AppController::onAudioEnd(const String& filePath) {
    LOG_INFO(AUDIO_TAG, "⏹ Audio playback finished: %s", filePath.c_str());
    if (m_deathController) {
        m_deathController->handleAudioFinished(std::string(filePath.c_str()));
        processControllerActions(m_deathController->pendingActions());
        m_deathController->clearActions();
    } else if (filePath.equals(m_initializationAudioPath)) {
        m_initializationPlayed = true;
    }

    if (m_skullAudioAnimator) {
        m_skullAudioAnimator->setPlaybackEnded(filePath);
    }
    if (m_skitSelector && filePath.startsWith("/audio/Skit")) {
        m_skitSelector->updateSkitPlayCount(filePath);
    }
}

void AppController::onAudioFrames(const String& filePath, const Frame* frames, int32_t frameCount) {
    if (!m_skullAudioAnimator || !m_audioPlayer || frameCount <= 0) {
        return;
    }
    const unsigned long playbackTime = m_audioPlayer->getPlaybackTime();
    m_skullAudioAnimator->processAudioFrames(frames, frameCount, filePath, playbackTime);
}

String AppController::sanitizePath(const String& path) {
    String result = path;
    result.trim();
    while (result.endsWith("/") && result.length() > 1) {
        result = result.substring(0, result.length() - 1);
    }
    return result;
}

#endif  // ARDUINO
