#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "audio_player.h"
#include "cli_command_router.h"
#include "fortune_generator.h"
#include "runtime/module_options.h"
#include "sd_card_manager.h"
#include "death_controller.h"

class AudioDirectorySelector;
class AudioPlannerAdapter;
class BluetoothController;
class CliService;
class FingerSensor;
class FortuneServiceAdapter;
class LightController;
class ManualCalibrationAdapter;
class OTAManager;
class PrinterStatusAdapter;
class RemoteDebugManager;
class SkullAudioAnimator;
class SkitSelector;
class ThermalPrinter;
class UARTController;
class WiFiManager;
class ServoController;

namespace infra {
class ILogSink;
class IRandomSource;
class ITimeProvider;
}  // namespace infra

class AppController {
public:
    struct HardwarePins {
        int eyeLed = 32;
        int mouthLed = 33;
        int servo = 23;
        int fingerSensor = 4;
        int printerTx = 18;
        int printerRx = 19;
        int uartMatterTx = 21;
        int uartMatterRx = 22;
    };

    struct ModuleOptions {
        bool enableCli = APP_ENABLE_CLI;
        bool enableContentSelection = APP_ENABLE_CONTENT_SELECTION;
        bool enablePrinter = APP_ENABLE_PRINTER;
        bool enableConnectivity = APP_ENABLE_CONNECTIVITY;
        bool enableWifi = APP_ENABLE_WIFI;
        bool enableOta = APP_ENABLE_OTA;
        bool enableRemoteDebug = APP_ENABLE_REMOTE_DEBUG;
        bool enableBluetooth = APP_ENABLE_BLUETOOTH;

        static ModuleOptions DefaultsFromBuildFlags();
        void normalize();
    };

    struct ModuleProviders {
        CliService* cliService = nullptr;
        CliCommandRouter* cliRouter = nullptr;
        AudioPlayer* audioPlayer = nullptr;
        AudioDirectorySelector* audioSelector = nullptr;
        SkitSelector* skitSelector = nullptr;
        ThermalPrinter* printer = nullptr;
        WiFiManager* wifi = nullptr;
        OTAManager* ota = nullptr;
        RemoteDebugManager* remoteDebug = nullptr;
        BluetoothController* bluetooth = nullptr;
    };

    ~AppController();

    AppController(const HardwarePins& pins,
                  infra::ITimeProvider& timeProvider,
                  infra::IRandomSource& randomSource,
                  infra::ILogSink* logSink,
                  ModuleOptions options,
                  ModuleProviders providers);

    bool setup();
    void loop();
    void enqueueCliCommand(const String& command);
    bool isInitialized() const { return m_initialized; }

private:
    class SerialPrinter;

    void setupLogging();
    void mountSdCard();
    void loadConfiguration();
    void initializeServo();
    void initializeAudio();
    void initializeBluetooth();
    void initializeUart();
    void initializePrinter();
    void initializeFingerSensor();
    void initializeDeathController();
    void initializeSkitSystems();
    void initializeConnectivity();
    void initializeCli();
    void queueInitializationAudio();

    void processControllerActions(const DeathController::ControllerActions& actions);
    void updatePrinterFaultIndicator();
    void breathingJawMovement();
    void handleUartCommand(UARTCommand cmd);
    void printFortuneToSerial(const String& fortune);
    void logAudioDirectoryTree(const char* path, int depth);
    void validateAudioDirectories();
    void testSkitSelection();
    void testCategorySelection(const char* directory, const char* label);
    int countWavFilesInDirectory(const char* directory);
    int computeServoMarginDegrees() const;
    int getServoClosedPosition() const;
    int getServoOpenPosition() const;
    std::vector<std::string> gatherFortuneCandidates(
        const String& configuredPath);
    void configureCliRouter();
    void updateConnectivity();
    void onAudioStart(const String& filePath);
    void onAudioEnd(const String& filePath);
    void onAudioFrames(const String& filePath, const Frame* frames, int32_t frameCount);

    HardwarePins m_pins;
    ModuleOptions m_options;
    ModuleProviders m_providers;

    infra::ITimeProvider& m_timeProvider;
    infra::IRandomSource& m_randomSource;
    infra::ILogSink* m_logSink;

    bool m_initialized = false;
    bool m_sdCardMounted = false;
    bool m_remoteDebugStreamingWasEnabled = true;
    bool m_mouthOpen = false;
    bool m_mouthPulseActive = false;
    bool m_printerFaultLatched = false;
    bool m_configLoaded = false;

    unsigned long m_fingerStableMs = 120;
    unsigned long m_fingerWaitMs = 6000;
    unsigned long m_snapDelayMinMs = 1000;
    unsigned long m_snapDelayMaxMs = 3000;
    unsigned long m_cooldownMs = 12000;

    unsigned long m_lastJawMovementTime = 0;
    bool m_initializationQueued = false;
    bool m_initializationPlayed = false;

    SDCardManager m_sdCardManager;
    SDCardContent m_sdCardContent;

    std::unique_ptr<LightController> m_lightController;
    std::unique_ptr<ServoController> m_servoController;
    std::unique_ptr<FingerSensor> m_fingerSensor;

    std::unique_ptr<AudioDirectorySelector> m_audioDirectorySelectorOwned;
    AudioDirectorySelector* m_audioDirectorySelector = nullptr;

    std::unique_ptr<SkitSelector> m_skitSelectorOwned;
    SkitSelector* m_skitSelector = nullptr;

    std::unique_ptr<AudioPlannerAdapter> m_audioPlannerAdapter;
    std::unique_ptr<AudioPlayer> m_audioPlayerOwned;
    AudioPlayer* m_audioPlayer = nullptr;

    std::unique_ptr<FortuneServiceAdapter> m_fortuneServiceAdapter;
    std::unique_ptr<PrinterStatusAdapter> m_printerStatusAdapter;
    std::unique_ptr<ManualCalibrationAdapter> m_manualCalibrationAdapter;

    std::unique_ptr<ThermalPrinter> m_thermalPrinterOwned;
    ThermalPrinter* m_thermalPrinter = nullptr;

    std::unique_ptr<BluetoothController> m_bluetoothControllerOwned;
    BluetoothController* m_bluetoothController = nullptr;

    std::unique_ptr<WiFiManager> m_wifiManagerOwned;
    WiFiManager* m_wifiManager = nullptr;

    std::unique_ptr<OTAManager> m_otaManagerOwned;
    OTAManager* m_otaManager = nullptr;

    std::unique_ptr<RemoteDebugManager> m_remoteDebugManagerOwned;
    RemoteDebugManager* m_remoteDebugManager = nullptr;

    std::unique_ptr<UARTController> m_uartControllerOwned;
    UARTController* m_uartController = nullptr;

    std::unique_ptr<CliService> m_cliServiceOwned;
    CliService* m_cliService = nullptr;

    std::unique_ptr<CliCommandRouter> m_cliRouterOwned;
    CliCommandRouter* m_cliRouter = nullptr;

    std::unique_ptr<DeathController> m_deathController;
    std::unique_ptr<SkullAudioAnimator> m_skullAudioAnimator;

    FortuneGenerator m_fortuneGenerator;

    String m_initializationAudioPath;
    String m_printerLogoPath;
    String m_fortunesJsonPath;
    std::vector<std::string> m_fortuneCandidates;

    unsigned long m_lastHandshakeReport = 0;

    class SerialPrinter : public CliCommandRouter::IPrinter {
    public:
        explicit SerialPrinter(Stream& stream);
        void print(const String& value) override;
        void println(const String& value) override;
        void println() override;
        void printf(const char* fmt, ...) override;
    private:
        Stream& m_stream;
    };

    std::unique_ptr<SerialPrinter> m_cliPrinter;

    static void audioStartThunk(const String& filePath);
    static void audioEndThunk(const String& filePath);
    static void audioFramesThunk(const String& filePath, const Frame* frames, int32_t frameCount);

    static AppController* s_instance;

    static String sanitizePath(const String& path);
};

#endif  // APP_CONTROLLER_H

