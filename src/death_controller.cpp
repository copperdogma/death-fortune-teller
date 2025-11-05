#include "death_controller.h"

#include <algorithm>

#include "infra/log_sink.h"

namespace {

constexpr const char *kTag = "DeathController";
constexpr uint32_t kTriggerDebounceMs = 2000;
constexpr uint32_t kMouthPulseDelayMs = 250;
constexpr uint32_t kFortunePrintMinDelayMs = 250;
constexpr uint32_t kFortunePrintMaxDelayMs = 1500;
constexpr float kManualCalibrationForceMultiplier = 10.0f;
constexpr uint32_t kManualCalibrationHoldMs = 3000;
constexpr uint32_t kManualCalibrationWaitMs = 5000;
constexpr uint32_t kManualCalibrationSettleMs = 1500;

const char *stateToString(DeathController::State state) {
    switch (state) {
        case DeathController::State::Idle: return "IDLE";
        case DeathController::State::PlayWelcome: return "PLAY_WELCOME";
        case DeathController::State::WaitForNear: return "WAIT_FOR_NEAR";
        case DeathController::State::PlayFingerPrompt: return "PLAY_FINGER_PROMPT";
        case DeathController::State::MouthOpenWaitFinger: return "MOUTH_OPEN_WAIT_FINGER";
        case DeathController::State::FingerDetected: return "FINGER_DETECTED";
        case DeathController::State::SnapWithFinger: return "SNAP_WITH_FINGER";
        case DeathController::State::SnapNoFinger: return "SNAP_NO_FINGER";
        case DeathController::State::FortuneFlow: return "FORTUNE_FLOW";
        case DeathController::State::FortuneDone: return "FORTUNE_DONE";
        case DeathController::State::Cooldown: return "COOLDOWN";
        case DeathController::State::ManualCalibration: return "MANUAL_CALIBRATION";
        default: return "UNKNOWN";
    }
}

const char *commandToString(UARTCommand cmd) {
    switch (cmd) {
        case UARTCommand::FAR_MOTION_TRIGGER: return "FAR_MOTION_TRIGGER";
        case UARTCommand::NEAR_MOTION_TRIGGER: return "NEAR_MOTION_TRIGGER";
        case UARTCommand::PLAY_WELCOME: return "PLAY_WELCOME";
        case UARTCommand::WAIT_FOR_NEAR: return "WAIT_FOR_NEAR";
        case UARTCommand::PLAY_FINGER_PROMPT: return "PLAY_FINGER_PROMPT";
        case UARTCommand::MOUTH_OPEN_WAIT_FINGER: return "MOUTH_OPEN_WAIT_FINGER";
        case UARTCommand::FINGER_DETECTED: return "FINGER_DETECTED";
        case UARTCommand::SNAP_WITH_FINGER: return "SNAP_WITH_FINGER";
        case UARTCommand::SNAP_NO_FINGER: return "SNAP_NO_FINGER";
        case UARTCommand::FORTUNE_FLOW: return "FORTUNE_FLOW";
        case UARTCommand::FORTUNE_DONE: return "FORTUNE_DONE";
        case UARTCommand::COOLDOWN: return "COOLDOWN";
        case UARTCommand::LEGACY_SET_MODE: return "LEGACY_SET_MODE";
        case UARTCommand::LEGACY_PING: return "LEGACY_PING";
        case UARTCommand::BOOT_HELLO: return "BOOT_HELLO";
        case UARTCommand::FABRIC_HELLO: return "FABRIC_HELLO";
        case UARTCommand::NONE:
        default:
            return "NONE";
    }
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

DeathController::State stateForCommand(UARTCommand cmd) {
    switch (cmd) {
        case UARTCommand::PLAY_WELCOME: return DeathController::State::PlayWelcome;
        case UARTCommand::WAIT_FOR_NEAR: return DeathController::State::WaitForNear;
        case UARTCommand::PLAY_FINGER_PROMPT: return DeathController::State::PlayFingerPrompt;
        case UARTCommand::MOUTH_OPEN_WAIT_FINGER: return DeathController::State::MouthOpenWaitFinger;
        case UARTCommand::FINGER_DETECTED: return DeathController::State::FingerDetected;
        case UARTCommand::SNAP_WITH_FINGER: return DeathController::State::SnapWithFinger;
        case UARTCommand::SNAP_NO_FINGER: return DeathController::State::SnapNoFinger;
        case UARTCommand::FORTUNE_FLOW: return DeathController::State::FortuneFlow;
        case UARTCommand::FORTUNE_DONE: return DeathController::State::FortuneDone;
        case UARTCommand::COOLDOWN: return DeathController::State::Cooldown;
        default: return DeathController::State::Idle;
    }
}

}  // namespace

DeathController::DeathController(const Dependencies &deps)
    : m_deps(deps) {
}

void DeathController::initialize(const ConfigSnapshot &config) {
    m_config = config;
    if (m_config.welcomeDir.empty()) m_config.welcomeDir = "/audio/welcome";
    if (m_config.fingerPromptDir.empty()) m_config.fingerPromptDir = "/audio/finger_prompt";
    if (m_config.fingerSnapDir.empty()) m_config.fingerSnapDir = "/audio/finger_snap";
    if (m_config.noFingerDir.empty()) m_config.noFingerDir = "/audio/no_finger";
    if (m_config.fortunePreambleDir.empty()) m_config.fortunePreambleDir = "/audio/fortune_preamble";
    if (m_config.fortuneFlowDir.empty()) m_config.fortuneFlowDir = "/audio/fortune_templates";
    if (m_config.fortuneDoneDir.empty()) m_config.fortuneDoneDir = "/audio/fortune_told";
    m_fortuneCandidates = m_config.fortuneCandidates;
    m_fortuneSource.clear();
    m_fortuneLoaded = false;

    if (m_config.fingerWaitMs == 0) m_config.fingerWaitMs = 6000;
    if (m_config.snapDelayMinMs == 0) m_config.snapDelayMinMs = 1000;
    if (m_config.snapDelayMaxMs == 0) m_config.snapDelayMaxMs = std::max<uint32_t>(m_config.snapDelayMinMs, 3000);
    if (m_config.snapDelayMaxMs < m_config.snapDelayMinMs) {
        std::swap(m_config.snapDelayMinMs, m_config.snapDelayMaxMs);
    }
    if (m_config.cooldownMs == 0) m_config.cooldownMs = 12000;

    m_fortuneCandidates = m_config.fortuneCandidates;
    if (m_fortuneCandidates.empty() && !m_config.fortuneFlowDir.empty()) {
        m_fortuneCandidates.push_back(m_config.fortuneFlowDir);
    }
    m_fortuneSource.clear();
    m_fortuneLoaded = false;

    m_stateEntryMs = now();
    m_lastTriggerMs = 0;
    m_mouthPulseActive = false;
    m_snapDelayScheduled = false;
    m_fortunePrintPending = false;
    m_fortuneGenerated = false;
    m_fortunePrintAttempted = false;
    m_fortunePrintSuccess = false;
    m_actions = ControllerActions{};
    m_fingerWaitStartMs = 0;
    m_fortunePrintPending = false;
    m_fortunePrintStartRequested = false;
    m_fortunePrintStartMs = 0;
    m_manualHoldActive = false;
    m_manualHoldSatisfied = false;
    m_manualHoldStartMs = 0;
    m_manualStage = ManualCalibrationStage::Idle;
    m_manualStageStartMs = now();
    m_manualCalibrateStartMs = 0;
    m_state = State::ManualCalibration;  // Sentinel to force transition actions.
    transitionTo(State::Idle, "Initialization");
}

void DeathController::update(uint32_t nowMs, const FingerReadout &finger) {
    (void)finger;

    switch (m_state) {
        case State::MouthOpenWaitFinger: {
            if (!m_mouthPulseActive && nowMs - m_stateEntryMs >= kMouthPulseDelayMs) {
                m_actions.requestMouthPulseEnable = true;
                m_mouthPulseActive = true;
            }

            if (finger.stable) {
                transitionTo(State::FingerDetected, "Finger stabilized");
                return;
            }

            if (m_fingerWaitStartMs > 0) {
                uint32_t elapsed = nowMs - m_fingerWaitStartMs;
                if (elapsed >= m_config.fingerWaitMs) {
                    infra::emitLog(infra::LogLevel::Info, kTag,
                                   "Finger wait timeout after %u ms (configured %u)",
                                   static_cast<unsigned int>(elapsed),
                                   static_cast<unsigned int>(m_config.fingerWaitMs));
                    transitionTo(State::SnapNoFinger, "Finger wait timeout");
                    return;
                }
            }
            break;
        }

        case State::FingerDetected: {
            if (!m_snapDelayScheduled) {
                scheduleSnapDelay();
            }

            if (m_snapDelayScheduled) {
                uint32_t elapsed = nowMs - m_snapDelayStartMs;
                if (elapsed >= m_snapDelayDurationMs) {
                    transitionTo(State::SnapWithFinger, "Snap delay elapsed");
                    return;
                }
            }

            if (!finger.detected) {
                if (nowMs - m_lastFingerRemovedWarnMs >= 1000) {
                    infra::emitLog(infra::LogLevel::Warn, kTag,
                                   "Finger removed after detection; continuing countdown");
                    m_lastFingerRemovedWarnMs = nowMs;
                }
            }
            break;
        }

        case State::Cooldown: {
            if (nowMs - m_stateEntryMs >= m_config.cooldownMs) {
                transitionTo(State::Idle, "Cooldown elapsed");
                return;
            }
            break;
        }

        case State::ManualCalibration: {
            switch (m_manualStage) {
                case ManualCalibrationStage::PreBlink: {
                    bool blinking = m_deps.manualCalibDriver ? m_deps.manualCalibDriver->isBlinking() : false;
                    if (!blinking) {
                        m_manualStage = ManualCalibrationStage::WaitBeforeCalibration;
                        m_manualStageStartMs = nowMs;
                        if (m_deps.manualCalibDriver) {
                            m_deps.manualCalibDriver->setWaitMode();
                        }
                        infra::emitLog(infra::LogLevel::Info, kTag,
                                       "Manual calibration: wait before calibration");
                    }
                    break;
                }

                case ManualCalibrationStage::WaitBeforeCalibration: {
                    if (nowMs - m_manualStageStartMs >= kManualCalibrationWaitMs) {
                        if (m_deps.manualCalibDriver) {
                            m_deps.manualCalibDriver->calibrateSensor();
                        }
                        m_manualCalibrateStartMs = nowMs;
                        m_manualStage = ManualCalibrationStage::Calibrating;
                        infra::emitLog(infra::LogLevel::Info, kTag,
                                       "Manual calibration: calibrating sensor");
                    }
                    break;
                }

                case ManualCalibrationStage::Calibrating: {
                    if (nowMs - m_manualCalibrateStartMs >= kManualCalibrationSettleMs) {
                        if (m_deps.manualCalibDriver) {
                            m_deps.manualCalibDriver->startCompletionBlink();
                        }
                        m_manualStage = ManualCalibrationStage::CompletionBlink;
                        infra::emitLog(infra::LogLevel::Info, kTag,
                                       "Manual calibration: completion blink");
                    }
                    break;
                }

                case ManualCalibrationStage::CompletionBlink: {
                    bool blinking = m_deps.manualCalibDriver ? m_deps.manualCalibDriver->isBlinking() : false;
                    if (!blinking) {
                        m_manualStage = ManualCalibrationStage::Idle;
                        transitionTo(State::Idle, "Manual calibration finished");
                    }
                    break;
                }

                case ManualCalibrationStage::Idle:
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    if (m_state == State::Idle) {
        const float threshold = finger.thresholdRatio * kManualCalibrationForceMultiplier;
        const bool strongTouch = (finger.thresholdRatio > 0.0f) && (finger.normalizedDelta >= threshold);

        if (strongTouch) {
            if (!m_manualHoldActive) {
                m_manualHoldActive = true;
                m_manualHoldStartMs = nowMs;
                m_manualHoldSatisfied = false;
                infra::emitLog(infra::LogLevel::Debug, kTag,
                               "Manual calibration hold started (delta=%.4f threshold=%.4f)",
                               finger.normalizedDelta,
                               threshold);
            } else if (!m_manualHoldSatisfied && nowMs - m_manualHoldStartMs >= kManualCalibrationHoldMs) {
                m_manualHoldSatisfied = true;
                infra::emitLog(infra::LogLevel::Debug, kTag,
                               "Manual calibration hold satisfied after %lu ms", static_cast<unsigned long>(nowMs - m_manualHoldStartMs));
                transitionTo(State::ManualCalibration, "Manual calibration requested");
                return;
            }
        } else {
            m_manualHoldActive = false;
            m_manualHoldSatisfied = false;
        }
    } else {
        m_manualHoldActive = false;
        m_manualHoldSatisfied = false;
    }

    if (m_fortunePrintPending && m_fortunePrintStartRequested) {
        uint32_t elapsed = nowMs - m_fortunePrintStartMs;
        if (!m_fortunePrintAttempted && elapsed >= kFortunePrintMinDelayMs) {
            requestFortunePrint();
        } else if (!m_fortunePrintAttempted && elapsed >= kFortunePrintMaxDelayMs) {
            infra::emitLog(infra::LogLevel::Warn, kTag, "Fortune print window elapsed without printer ready");
            m_fortunePrintPending = false;
        }
    }
}

void DeathController::handleUartCommand(UARTCommand command) {
    uint32_t nowMs = now();
    if (isTriggerCommand(command)) {
        if (nowMs - m_lastTriggerMs < kTriggerDebounceMs) {
            infra::emitLog(infra::LogLevel::Warn, kTag, "Trigger command debounced");
            return;
        }

        if (command == UARTCommand::FAR_MOTION_TRIGGER) {
            if (isBusy()) {
                infra::emitLog(infra::LogLevel::Warn, kTag,
                               "Ignoring FAR trigger while busy (state=%s)", stateToString(m_state));
                return;
            }
            m_lastTriggerMs = nowMs;
            transitionTo(State::PlayWelcome, "FAR trigger");
            return;
        }

        if (command == UARTCommand::NEAR_MOTION_TRIGGER) {
            if (m_state != State::WaitForNear) {
                infra::emitLog(infra::LogLevel::Warn, kTag,
                               "NEAR trigger dropped in state %s", stateToString(m_state));
                return;
            }
            m_lastTriggerMs = nowMs;
            transitionTo(State::PlayFingerPrompt, "NEAR trigger");
            return;
        }
    }

    if (isForcedStateCommand(command)) {
        State target = stateForCommand(command);
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "State forcing command received: %s -> %s",
                       commandToString(command),
                       stateToString(target));
        transitionTo(target, "Forced via UART command");
        return;
    }
}

void DeathController::handleAudioStarted(const std::string &clipPath) {
    if (m_state == State::FortuneFlow) {
        if (clipPath.find(m_config.fortunePreambleDir) == 0) {
            m_fortunePrintStartRequested = true;
            m_fortunePrintStartMs = now();
            infra::emitLog(infra::LogLevel::Info, kTag, "Fortune preamble started; scheduling printer window");
        }
    }
}

void DeathController::handleAudioFinished(const std::string &completedClip) {
    (void)completedClip;

    switch (m_state) {
        case State::PlayWelcome:
            transitionTo(State::WaitForNear, "Welcome audio finished");
            break;

        case State::PlayFingerPrompt:
            transitionTo(State::MouthOpenWaitFinger, "Finger prompt finished");
            break;

        case State::SnapWithFinger:
        case State::SnapNoFinger:
            transitionTo(State::FortuneFlow, "Snap sequence finished");
            break;

        case State::FortuneFlow:
            transitionTo(State::FortuneDone, "Fortune flow audio finished");
            break;

        case State::FortuneDone:
            transitionTo(State::Cooldown, "Fortune done sequence complete");
            break;

        default:
            break;
    }
}

const DeathController::ControllerActions &DeathController::pendingActions() const {
    return m_actions;
}

void DeathController::clearActions() {
    m_actions = ControllerActions{};
}

void DeathController::transitionTo(State nextState, const char *reason) {
    if (m_state == nextState) {
        infra::emitLog(infra::LogLevel::Info, kTag,
                       "State %s already active; ignoring transition (%s)",
                       stateToString(nextState),
                       reason ? reason : "no reason");
        return;
    }

    const uint32_t nowMs = now();
    infra::emitLog(infra::LogLevel::Info, kTag,
                   "Transition %s -> %s (%s)",
                   stateToString(m_state),
                   stateToString(nextState),
                   reason ? reason : "no reason");

    m_state = nextState;
    m_stateEntryMs = nowMs;
    m_actions = ControllerActions{};
    m_mouthPulseActive = false;
    m_snapDelayScheduled = false;
    m_snapDelayDurationMs = 0;
    m_snapDelayStartMs = 0;
    m_lastFingerRemovedWarnMs = 0;

    switch (nextState) {
        case State::Idle:
            m_actions.resetFortuneState = true;
            m_actions.requestMouthClose = true;
            m_actions.requestLedIdle = true;
            m_actions.requestMouthPulseDisable = true;
            m_fortuneGenerated = false;
            m_fortunePrintPending = false;
            m_fortunePrintAttempted = false;
            m_fortunePrintSuccess = false;
            m_fortunePrintStartRequested = false;
            m_fortunePrintStartMs = 0;
            m_activeFortune.clear();
            break;

        case State::PlayWelcome:
            m_actions.resetFortuneState = true;
            m_actions.requestMouthClose = true;
            m_actions.requestLedPrompt = true;
            if (!queueAudioFromDirectory(m_config.welcomeDir, "welcome skit")) {
                transitionTo(State::WaitForNear, "Welcome audio missing");
                return;
            }
            break;

        case State::WaitForNear:
            m_actions.requestMouthClose = true;
            m_actions.requestLedIdle = true;
            break;

        case State::PlayFingerPrompt:
            m_actions.requestLedPrompt = true;
            if (!queueAudioFromDirectory(m_config.fingerPromptDir, "finger prompt")) {
                transitionTo(State::MouthOpenWaitFinger, "Finger prompt audio missing");
                return;
            }
            break;

        case State::MouthOpenWaitFinger:
            m_actions.requestMouthOpen = true;
            m_actions.requestLedPrompt = true;
            m_actions.requestMouthPulseDisable = true;
            m_fingerWaitStartMs = nowMs;
            break;

        case State::FingerDetected:
            m_actions.requestLedFingerDetected = true;
            m_actions.requestMouthOpen = true;
            scheduleSnapDelay();
            break;

        case State::SnapWithFinger:
            m_actions.requestMouthClose = true;
            m_actions.requestLedIdle = true;
            if (!queueAudioFromDirectory(m_config.fingerSnapDir, "snap with finger")) {
                transitionTo(State::FortuneFlow, "Snap with finger audio missing");
                return;
            }
            break;

        case State::SnapNoFinger:
            m_actions.requestMouthClose = true;
            m_actions.requestLedIdle = true;
            if (!queueAudioFromDirectory(m_config.noFingerDir, "no finger response")) {
                transitionTo(State::FortuneFlow, "Snap no finger audio missing");
                return;
            }
            break;

        case State::FortuneFlow:
            m_actions.requestMouthOpen = true;
            m_actions.requestLedPrompt = true;
            ensureFortuneGenerated();
            if (queueAudioFromDirectory(m_config.fortunePreambleDir, "fortune preamble")) {
                m_fortunePrintPending = true;
                m_fortunePrintStartRequested = false;
                m_fortunePrintStartMs = 0;
            } else {
                // No preamble; print immediately and advance.
                requestFortunePrint();
                bool queuePrint = m_actions.queueFortunePrint;
                std::string fortuneText = m_actions.fortuneText;
                transitionTo(State::FortuneDone, "Fortune preamble missing");
                if (queuePrint) {
                    m_actions.queueFortunePrint = true;
                    m_actions.fortuneText = fortuneText;
                }
                return;
            }
            break;

        case State::FortuneDone:
            m_actions.requestMouthClose = true;
            m_actions.requestLedIdle = true;
            if (!queueAudioFromDirectory(m_config.fortuneDoneDir, "fortune done")) {
                transitionTo(State::Cooldown, "Fortune done audio missing");
                return;
            }
            break;

        case State::Cooldown:
            m_actions.requestMouthClose = true;
            m_actions.requestLedIdle = true;
            if (m_fortunePrintAttempted) {
                infra::emitLog(
                    infra::LogLevel::Info, kTag,
                    "Fortune cycle summary — printed=%s text=\"%s\"",
                    m_fortunePrintSuccess ? "true" : "false",
                    m_activeFortune.c_str());
            }
            break;

        case State::ManualCalibration:
            m_manualStage = ManualCalibrationStage::PreBlink;
            m_manualStageStartMs = nowMs;
            m_manualCalibrateStartMs = 0;
            if (m_deps.manualCalibDriver) {
                m_deps.manualCalibDriver->startPreBlink();
            }
            m_manualHoldActive = false;
            m_manualHoldSatisfied = false;
            infra::emitLog(infra::LogLevel::Info, kTag, "Manual calibration: pre-blink");
            break;
    }
}

bool DeathController::queueAudioFromDirectory(const std::string &directory, const char *label) {
    if (!m_deps.audioPlanner) {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Audio planner missing; cannot queue %s", label ? label : "(unknown)");
        return false;
    }
    if (directory.empty()) {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Audio directory empty for %s", label ? label : "(unknown)");
        return false;
    }
    if (!m_deps.audioPlanner->hasAvailableClip(directory, label)) {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "No audio available in %s for %s", directory.c_str(), label ? label : "(unknown)");
        return false;
    }
    std::string clip = m_deps.audioPlanner->pickClip(directory, label);
    if (clip.empty()) {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Audio planner returned empty clip for %s", label ? label : "(unknown)");
        return false;
    }
    m_actions.audioToQueue.push_back(clip);
    infra::emitLog(infra::LogLevel::Info, kTag,
                   "Queued %s clip: %s", label ? label : "(audio)", clip.c_str());
    return true;
}

void DeathController::ensureFortuneGenerated() {
    if (m_fortuneGenerated) {
        return;
    }

    if (!m_deps.fortuneService || !ensureFortuneTemplatesLoaded()) {
        m_activeFortune = "The spirits are silent...";
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Fortune templates unavailable; using fallback fortune");
    } else {
        m_activeFortune = m_deps.fortuneService->generateFortune();
        if (m_activeFortune.empty()) {
            m_activeFortune = "The spirits are silent...";
        }
    }

    infra::emitLog(infra::LogLevel::Info, kTag,
                   "Generated fortune: %s", m_activeFortune.c_str());
    m_actions.fortuneText = m_activeFortune;
    m_fortuneGenerated = true;
    m_fortunePrintAttempted = false;
    m_fortunePrintSuccess = false;
}

void DeathController::requestFortunePrint() {
    if (!m_fortuneGenerated) {
        ensureFortuneGenerated();
    }

    m_fortunePrintAttempted = true;
    m_fortunePrintPending = false;
    if (!m_deps.printerStatus) {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Printer status seam missing; fortune will not be printed");
        m_fortunePrintSuccess = false;
        return;
    }

    if (!m_deps.printerStatus->isReady()) {
        infra::emitLog(infra::LogLevel::Warn, kTag,
                       "Printer not ready; skipping fortune print");
        m_fortunePrintSuccess = false;
        return;
    }

    m_actions.queueFortunePrint = true;
    m_actions.fortuneText = m_activeFortune;
    m_fortunePrintSuccess = true;  // Assume success; hardware adapter will update if needed.
    infra::emitLog(infra::LogLevel::Info, kTag,
                   "Thermal printer job requested");
}

void DeathController::scheduleSnapDelay() {
    if (m_snapDelayScheduled) {
        return;
    }

    uint32_t minMs = m_config.snapDelayMinMs;
    uint32_t maxMs = std::max(m_config.snapDelayMaxMs, minMs);
    if (m_deps.random) {
        int duration = m_deps.random->nextInt(static_cast<int>(minMs), static_cast<int>(maxMs) + 1);
        m_snapDelayDurationMs = static_cast<uint32_t>(duration);
    } else {
        m_snapDelayDurationMs = minMs;
    }
    m_snapDelayStartMs = now();
    m_snapDelayScheduled = true;
    infra::emitLog(infra::LogLevel::Info, kTag,
                   "Snap delay scheduled (%u ms)", static_cast<unsigned int>(m_snapDelayDurationMs));
}

bool DeathController::ensureFortuneTemplatesLoaded() {
    if (m_fortuneLoaded && !m_fortuneSource.empty()) {
        return true;
    }

    if (!m_deps.fortuneService) {
        return false;
    }

    if (m_fortuneCandidates.empty()) {
        return false;
    }

    size_t count = m_fortuneCandidates.size();
    size_t startIndex = 0;
    if (m_deps.random) {
        startIndex = static_cast<size_t>(m_deps.random->nextInt(0, static_cast<int>(count)));
    }

    for (size_t attempt = 0; attempt < count; ++attempt) {
        size_t idx = (startIndex + attempt) % count;
        const std::string &candidate = m_fortuneCandidates[idx];
        if (candidate.empty()) {
            continue;
        }
        if (m_deps.fortuneService->ensureLoaded(candidate)) {
            m_fortuneLoaded = true;
            m_fortuneSource = candidate;
            infra::emitLog(infra::LogLevel::Info, kTag,
                           "Fortune templates loaded from %s", candidate.c_str());
            return true;
        }
    }

    infra::emitLog(infra::LogLevel::Warn, kTag,
                   "Could not load fortune templates from configured candidates");
    return false;
}

uint32_t DeathController::now() const {
    return m_deps.time ? m_deps.time->nowMillis() : 0;
}

bool DeathController::isBusy() const {
    return m_state != State::Idle;
}
