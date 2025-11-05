#ifndef DEATH_CONTROLLER_H
#define DEATH_CONTROLLER_H

#include <stdint.h>

#include <string>
#include <vector>

#include "infra/log_sink.h"
#include "infra/random_source.h"
#include "infra/time_provider.h"
#include "uart_controller.h"

class DeathController {
public:
    enum class State {
        Idle,
        PlayWelcome,
        WaitForNear,
        PlayFingerPrompt,
        MouthOpenWaitFinger,
        FingerDetected,
        SnapWithFinger,
        SnapNoFinger,
        FortuneFlow,
        FortuneDone,
        Cooldown,
        ManualCalibration
    };

    struct ConfigSnapshot {
        uint32_t fingerStableMs = 0;
        uint32_t fingerWaitMs = 0;
        uint32_t snapDelayMinMs = 0;
        uint32_t snapDelayMaxMs = 0;
        uint32_t cooldownMs = 0;
        std::string welcomeDir;
        std::string fingerPromptDir;
        std::string fingerSnapDir;
        std::string noFingerDir;
        std::string fortunePreambleDir;
        std::string fortuneFlowDir;
        std::string fortuneDoneDir;
        std::vector<std::string> fortuneCandidates;
    };

    struct FingerReadout {
        bool detected = false;
        bool stable = false;
        float normalizedDelta = 0.0f;
        float thresholdRatio = 0.0f;
    };

    struct ControllerActions {
        std::vector<std::string> audioToQueue;
        bool requestMouthOpen = false;
        bool requestMouthClose = false;
        bool requestMouthPulseEnable = false;
        bool requestMouthPulseDisable = false;
        bool requestLedPrompt = false;
        bool requestLedIdle = false;
        bool requestLedFingerDetected = false;
        bool queueFortunePrint = false;
        std::string fortuneText;
        bool resetFortuneState = false;
        bool requestRemoteDebugPause = false;
        bool requestRemoteDebugResume = false;
    };

    class IAudioPlanner {
    public:
        virtual ~IAudioPlanner() = default;
        virtual bool hasAvailableClip(const std::string& directory, const char* label = nullptr) = 0;
        virtual std::string pickClip(const std::string& directory, const char* label = nullptr) = 0;
        virtual bool isAudioPlaying() const = 0;
    };

    class IFortuneService {
    public:
        virtual ~IFortuneService() = default;
        virtual bool ensureLoaded(const std::string& path) = 0;
        virtual std::string generateFortune() = 0;
    };

    class IPrinterStatus {
    public:
        virtual ~IPrinterStatus() = default;
        virtual bool isReady() const = 0;
    };

    class IManualCalibrationDriver {
    public:
        virtual ~IManualCalibrationDriver() = default;
        virtual void startPreBlink() = 0;
        virtual void setWaitMode() = 0;
        virtual void calibrateSensor() = 0;
        virtual void startCompletionBlink() = 0;
        virtual bool isBlinking() const = 0;
    };

    struct Dependencies {
        infra::ITimeProvider* time = nullptr;
        infra::IRandomSource* random = nullptr;
        infra::ILogSink* log = nullptr;
        IAudioPlanner* audioPlanner = nullptr;
        IFortuneService* fortuneService = nullptr;
        IPrinterStatus* printerStatus = nullptr;
        IManualCalibrationDriver* manualCalibDriver = nullptr;
    };

    explicit DeathController(const Dependencies& deps);

    void initialize(const ConfigSnapshot& config);
    void update(uint32_t nowMs, const FingerReadout& finger);
    void handleUartCommand(UARTCommand command);
    void handleAudioStarted(const std::string& clipPath);
    void handleAudioFinished(const std::string& completedClip);
    const ControllerActions& pendingActions() const;
    void clearActions();

    State state() const { return m_state; }

private:
    void transitionTo(State nextState, const char* reason);
    bool queueAudioFromDirectory(const std::string& directory, const char* label);
    void ensureFortuneGenerated();
    void requestFortunePrint();
    void scheduleSnapDelay();
    bool ensureFortuneTemplatesLoaded();
    uint32_t now() const;
    bool isBusy() const;

    enum class ManualCalibrationStage {
        Idle,
        PreBlink,
        WaitBeforeCalibration,
        Calibrating,
        CompletionBlink
    };

    Dependencies m_deps;
    ConfigSnapshot m_config;
    ControllerActions m_actions;

    State m_state = State::Idle;
    uint32_t m_stateEntryMs = 0;
    uint32_t m_lastTriggerMs = 0;
    bool m_mouthPulseActive = false;
    uint32_t m_fingerWaitStartMs = 0;
    uint32_t m_snapDelayStartMs = 0;
    uint32_t m_snapDelayDurationMs = 0;
    bool m_snapDelayScheduled = false;
    uint32_t m_lastFingerRemovedWarnMs = 0;
    bool m_fortuneGenerated = false;
    std::string m_activeFortune;
    bool m_fortunePrintAttempted = false;
    bool m_fortunePrintSuccess = false;
    bool m_fortunePrintPending = false;
    bool m_fortunePrintStartRequested = false;
    uint32_t m_fortunePrintStartMs = 0;
    bool m_manualHoldActive = false;
    bool m_manualHoldSatisfied = false;
    uint32_t m_manualHoldStartMs = 0;
    ManualCalibrationStage m_manualStage = ManualCalibrationStage::Idle;
    uint32_t m_manualStageStartMs = 0;
    uint32_t m_manualCalibrateStartMs = 0;
    std::vector<std::string> m_fortuneCandidates;
    std::string m_fortuneSource;
    bool m_fortuneLoaded = false;
};

#endif  // DEATH_CONTROLLER_H
