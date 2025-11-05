#include <unity.h>

#include <map>
#include <string>
#include <vector>

#include "death_controller.h"
#include "infra/log_sink.h"
#include "fake_log_sink.h"

namespace {

class FakeTimeProvider : public infra::ITimeProvider {
public:
    uint32_t nowMillis() const override { return currentMs; }
    uint64_t nowMicros() const override { return static_cast<uint64_t>(currentMs) * 1000ULL; }

    uint32_t currentMs = 0;
};

class FakeRandomSource : public infra::IRandomSource {
public:
    int nextInt(int minInclusive, int maxExclusive) override {
        (void)minInclusive;
        (void)maxExclusive;
        return forcedValue;
    }

    int forcedValue = 0;
};

class FakeAudioPlanner : public DeathController::IAudioPlanner {
public:
    void addDirectory(const std::string &directory, const std::vector<std::string> &clips) {
        catalog[directory] = clips;
    }

    bool hasAvailableClip(const std::string &directory, const char *label = nullptr) override {
        auto it = catalog.find(directory);
        (void)label;
        return it != catalog.end() && !it->second.empty();
    }

    std::string pickClip(const std::string &directory, const char *label = nullptr) override {
        lastRequestedDirectory = directory;
        (void)label;
        auto it = catalog.find(directory);
        if (it == catalog.end() || it->second.empty()) {
            return {};
        }
        return it->second.front();
    }

    bool isAudioPlaying() const override { return false; }

    std::string lastRequestedDirectory;
    std::map<std::string, std::vector<std::string>> catalog;

    bool hasDirectory(const std::string &directory) const {
        return catalog.find(directory) != catalog.end();
    }
};

class FakeFortuneService : public DeathController::IFortuneService {
public:
    bool ensureLoaded(const std::string &path) override {
        ensureLoadedRequests.push_back(path);
        return shouldLoadSucceed;
    }

    std::string generateFortune() override {
        generateCalls++;
        return nextFortuneText;
    }

    bool shouldLoadSucceed = true;
    std::string nextFortuneText = "Prophecy!";
    std::vector<std::string> ensureLoadedRequests;
    int generateCalls = 0;
};

class FakePrinterStatus : public DeathController::IPrinterStatus {
public:
    bool isReady() const override { return ready; }
    bool ready = true;
};

class FakeManualCalibrationDriver : public DeathController::IManualCalibrationDriver {
public:
    void startPreBlink() override {
        preBlinkCalls++;
        blinking = true;
    }

    void setWaitMode() override {
        waitModeCalls++;
    }

    void calibrateSensor() override {
        calibrateCalls++;
    }

    void startCompletionBlink() override {
        completionBlinkCalls++;
        blinking = true;
    }

    bool isBlinking() const override {
        return blinking;
    }

    void finishBlink() { blinking = false; }

    int preBlinkCalls = 0;
    int waitModeCalls = 0;
    int calibrateCalls = 0;
    int completionBlinkCalls = 0;
    bool blinking = false;
};

struct TestHarness {
    FakeTimeProvider time;
    FakeRandomSource random;
    FakeAudioPlanner audio;
    FakeFortuneService fortune;
    FakePrinterStatus printer;
    FakeManualCalibrationDriver manual;
    FakeLogSink log;
    DeathController::Dependencies deps;
    DeathController controller;

    TestHarness()
        : deps{&time, &random, &log, &audio, &fortune, &printer, &manual},
          controller(deps) {
        infra::setLogSink(&log);
    }

    ~TestHarness() {
        infra::setLogSink(nullptr);
    }

    DeathController::ConfigSnapshot defaultConfig() {
        DeathController::ConfigSnapshot config;
        config.fingerStableMs = 120;
        config.fingerWaitMs = 6000;
        config.snapDelayMinMs = 1000;
        config.snapDelayMaxMs = 2000;
        config.cooldownMs = 12000;
        config.welcomeDir = "/audio/welcome";
        config.fingerPromptDir = "/audio/finger_prompt";
        config.fingerSnapDir = "/audio/finger_snap";
        config.noFingerDir = "/audio/no_finger";
        config.fortunePreambleDir = "/audio/fortune_preamble";
        config.fortuneFlowDir = "/printer/fortunes.json";
        config.fortuneDoneDir = "/audio/fortune_told";
        config.fortuneCandidates = {"/printer/fortunes.json"};
        return config;
    }
};

}  // namespace

void setUp(void) {}
void tearDown(void) {}

static void test_far_trigger_queues_welcome_audio(void) {
    TestHarness harness;
    harness.audio.addDirectory("/audio/welcome", {"/audio/welcome/hello.wav"});

    auto config = harness.defaultConfig();
    harness.controller.initialize(config);
    harness.controller.clearActions();  // Ignore initial idle transition intents.
    harness.log.clear();
    harness.time.currentMs = 5000;
    harness.controller.handleUartCommand(UARTCommand::FAR_MOTION_TRIGGER);

    const auto &actions = harness.controller.pendingActions();
    bool sawQueuedLog = false;
    for (const auto &entry : harness.log.entries) {
        if (entry.tag == "DeathController" && entry.message.find("Queued") != std::string::npos) {
            sawQueuedLog = true;
            break;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(sawQueuedLog, "Expected log entry for queued welcome audio");
    TEST_ASSERT_EQUAL(DeathController::State::PlayWelcome, harness.controller.state());
    TEST_ASSERT_EQUAL(1, static_cast<int>(actions.audioToQueue.size()));
    TEST_ASSERT_EQUAL_STRING("/audio/welcome/hello.wav", actions.audioToQueue.front().c_str());
    TEST_ASSERT_TRUE(actions.requestLedPrompt);
    TEST_ASSERT_TRUE(actions.requestMouthClose);
}

static void test_near_trigger_requires_wait_for_near(void) {
    TestHarness harness;
    harness.audio.addDirectory("/audio/welcome", {"/audio/welcome/hello.wav"});
    harness.audio.addDirectory("/audio/finger_prompt", {"/audio/finger_prompt/prompt.wav"});

    harness.controller.initialize(harness.defaultConfig());
    harness.controller.clearActions();

    TEST_ASSERT_TRUE(harness.audio.hasDirectory("/audio/welcome"));
    harness.time.currentMs = 5000;
    harness.controller.handleUartCommand(UARTCommand::FAR_MOTION_TRIGGER);
    harness.controller.clearActions();

    // NEAR trigger before welcome completes should be dropped.
    harness.time.currentMs += 2500;
    harness.time.currentMs += 50;
    harness.controller.handleUartCommand(UARTCommand::NEAR_MOTION_TRIGGER);
    TEST_ASSERT_EQUAL(DeathController::State::PlayWelcome, harness.controller.state());

    // Simulate welcome audio completion to move to WAIT_FOR_NEAR.
    harness.controller.handleAudioFinished("/audio/welcome/hello.wav");
    harness.controller.clearActions();
    TEST_ASSERT_EQUAL(DeathController::State::WaitForNear, harness.controller.state());
    harness.time.currentMs += 5000;
    harness.controller.handleUartCommand(UARTCommand::NEAR_MOTION_TRIGGER);
    const auto &actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL(DeathController::State::PlayFingerPrompt, harness.controller.state());
    TEST_ASSERT_EQUAL_STRING("/audio/finger_prompt/prompt.wav", actions.audioToQueue.front().c_str());
    TEST_ASSERT_TRUE(actions.requestLedPrompt);
}

static void seedDefaultAudioClips(TestHarness &harness) {
    harness.audio.addDirectory("/audio/welcome", {"/audio/welcome/hello.wav"});
    harness.audio.addDirectory("/audio/finger_prompt", {"/audio/finger_prompt/prompt.wav"});
    harness.audio.addDirectory("/audio/finger_snap", {"/audio/finger_snap/snap.wav"});
    harness.audio.addDirectory("/audio/no_finger", {"/audio/no_finger/nope.wav"});
    harness.audio.addDirectory("/audio/fortune_preamble", {"/audio/fortune_preamble/preamble.wav"});
    harness.audio.addDirectory("/audio/fortune_told", {"/audio/fortune_told/done.wav"});
}

static DeathController::ControllerActions driveFortunePrintAttempt(TestHarness &harness,
                                                                  bool printerReady,
                                                                  const DeathController::ConfigSnapshot *configOverride = nullptr) {
    DeathController::ConfigSnapshot config = configOverride ? *configOverride : harness.defaultConfig();
    harness.random.forcedValue = static_cast<int>(config.snapDelayMinMs);
    harness.controller.initialize(config);
    harness.controller.clearActions();
    harness.time.currentMs = 5000;

    harness.controller.handleUartCommand(UARTCommand::FAR_MOTION_TRIGGER);
    auto actions = harness.controller.pendingActions();
    if (actions.audioToQueue.empty()) {
        TEST_FAIL_MESSAGE(harness.log.entries.empty() ? "No log entries captured" : harness.log.entries.back().message.c_str());
    }
    TEST_ASSERT_EQUAL(DeathController::State::PlayWelcome, harness.controller.state());
    TEST_ASSERT_EQUAL_STRING("/audio/welcome/hello.wav", actions.audioToQueue.front().c_str());
    harness.controller.clearActions();

    harness.controller.handleAudioFinished("/audio/welcome/hello.wav");
    TEST_ASSERT_EQUAL(DeathController::State::WaitForNear, harness.controller.state());
    harness.controller.clearActions();

    harness.time.currentMs += 2500;
    harness.controller.handleUartCommand(UARTCommand::NEAR_MOTION_TRIGGER);
    actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL(DeathController::State::PlayFingerPrompt, harness.controller.state());
    TEST_ASSERT_EQUAL_STRING("/audio/finger_prompt/prompt.wav", actions.audioToQueue.front().c_str());
    harness.controller.clearActions();

    harness.controller.handleAudioFinished("/audio/finger_prompt/prompt.wav");
    TEST_ASSERT_EQUAL(DeathController::State::MouthOpenWaitFinger, harness.controller.state());
    harness.controller.clearActions();

    DeathController::FingerReadout readout{};
    readout.detected = true;
    readout.stable = true;
    harness.time.currentMs += 50;
    harness.controller.update(harness.time.currentMs, readout);
    TEST_ASSERT_EQUAL(DeathController::State::FingerDetected, harness.controller.state());
    harness.controller.clearActions();

    harness.time.currentMs += config.snapDelayMinMs;
    harness.controller.update(harness.time.currentMs, readout);
    TEST_ASSERT_EQUAL(DeathController::State::SnapWithFinger, harness.controller.state());
    actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL_STRING("/audio/finger_snap/snap.wav", actions.audioToQueue.front().c_str());
    harness.controller.clearActions();

    harness.controller.handleAudioFinished("/audio/finger_snap/snap.wav");
    TEST_ASSERT_EQUAL(DeathController::State::FortuneFlow, harness.controller.state());
    actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL(1, static_cast<int>(actions.audioToQueue.size()));
    TEST_ASSERT_FALSE(actions.fortuneText.empty());
    harness.controller.clearActions();

    harness.controller.handleAudioStarted("/audio/fortune_preamble/preamble.wav");
    harness.controller.clearActions();

    harness.printer.ready = printerReady;
    harness.time.currentMs += 300;
    DeathController::FingerReadout idle{};
    harness.controller.update(harness.time.currentMs, idle);
    DeathController::ControllerActions finalActions = harness.controller.pendingActions();
    return finalActions;
}

static void test_printer_not_ready_skips_queue(void) {
    TestHarness harness;
    seedDefaultAudioClips(harness);
    harness.fortune.nextFortuneText = "Ghosts are busy.";

    DeathController::ControllerActions actions = driveFortunePrintAttempt(harness, false);
    TEST_ASSERT_FALSE(actions.queueFortunePrint);
}

static void test_printer_ready_queues_print(void) {
    TestHarness harness;
    seedDefaultAudioClips(harness);
    harness.fortune.nextFortuneText = "Beware the moon.";

    DeathController::ControllerActions actions = driveFortunePrintAttempt(harness, true);
    TEST_ASSERT_TRUE(actions.queueFortunePrint);
    TEST_ASSERT_EQUAL_STRING("Beware the moon.", actions.fortuneText.c_str());
}

static void driveToCooldown(TestHarness &harness, const DeathController::ConfigSnapshot &config) {
    driveFortunePrintAttempt(harness, true, &config);
    harness.controller.clearActions();
    harness.controller.handleAudioFinished("/audio/fortune_preamble/preamble.wav");
    harness.controller.clearActions();
    harness.controller.handleAudioFinished("/audio/fortune_told/done.wav");
    harness.controller.clearActions();
    TEST_ASSERT_EQUAL(DeathController::State::Cooldown, harness.controller.state());
}

static void test_finger_timeout_transitions_to_no_finger(void) {
    TestHarness harness;
    seedDefaultAudioClips(harness);

    auto config = harness.defaultConfig();
    config.fingerWaitMs = 1000;
    harness.controller.initialize(config);
    harness.controller.clearActions();
    harness.time.currentMs = 5000;

    harness.controller.handleUartCommand(UARTCommand::FAR_MOTION_TRIGGER);
    harness.controller.clearActions();
    harness.controller.handleAudioFinished("/audio/welcome/hello.wav");
    harness.controller.clearActions();
    harness.time.currentMs += 2500;
    harness.controller.handleUartCommand(UARTCommand::NEAR_MOTION_TRIGGER);
    harness.controller.clearActions();
    harness.controller.handleAudioFinished("/audio/finger_prompt/prompt.wav");
    harness.controller.clearActions();

    DeathController::FingerReadout readout{};
    readout.detected = false;
    readout.stable = false;
    harness.time.currentMs += config.fingerWaitMs + 10;
    harness.controller.update(harness.time.currentMs, readout);
    auto actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL(DeathController::State::SnapNoFinger, harness.controller.state());
    TEST_ASSERT_EQUAL(1, static_cast<int>(actions.audioToQueue.size()));
    TEST_ASSERT_EQUAL_STRING("/audio/no_finger/nope.wav", actions.audioToQueue.front().c_str());
    TEST_ASSERT_TRUE(actions.requestLedIdle);
    TEST_ASSERT_TRUE(actions.requestMouthClose);
}

static void test_cooldown_transitions_to_idle_after_timeout(void) {
    TestHarness harness;
    seedDefaultAudioClips(harness);
    auto config = harness.defaultConfig();
    config.cooldownMs = 500;
    harness.controller.initialize(config);
    harness.controller.clearActions();
    harness.time.currentMs = 9000;

    driveToCooldown(harness, config);
    harness.time.currentMs += config.cooldownMs + 20;
    harness.controller.update(harness.time.currentMs, {});
    auto actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL(DeathController::State::Idle, harness.controller.state());
    TEST_ASSERT_TRUE(actions.resetFortuneState);
    TEST_ASSERT_TRUE(actions.requestLedIdle);
    TEST_ASSERT_TRUE(actions.requestMouthClose);
}

static void test_manual_calibration_trigger_after_hold(void) {
    TestHarness harness;
    harness.controller.initialize(harness.defaultConfig());
    harness.controller.clearActions();

    DeathController::FingerReadout readout{};
    readout.detected = true;
    readout.stable = true;
    readout.thresholdRatio = 0.02f;
    readout.normalizedDelta = 0.25f; // strong touch

    harness.time.currentMs = 1000;
    harness.controller.update(harness.time.currentMs, readout);
    harness.controller.clearActions();

    harness.time.currentMs += 3100; // exceed hold ms
    harness.controller.update(harness.time.currentMs, readout);
    TEST_ASSERT_EQUAL(DeathController::State::ManualCalibration, harness.controller.state());
    TEST_ASSERT_EQUAL(1, harness.manual.preBlinkCalls);
    TEST_ASSERT_TRUE(harness.manual.blinking);

    harness.manual.finishBlink();
    harness.time.currentMs += 100;
    harness.controller.update(harness.time.currentMs, {});
    TEST_ASSERT_EQUAL(1, harness.manual.waitModeCalls);

    harness.time.currentMs += 5000;
    harness.controller.update(harness.time.currentMs, {});
    TEST_ASSERT_EQUAL(1, harness.manual.calibrateCalls);

    harness.time.currentMs += 1500;
    harness.controller.update(harness.time.currentMs, {});
    TEST_ASSERT_EQUAL(1, harness.manual.completionBlinkCalls);
    TEST_ASSERT_TRUE(harness.manual.blinking);

    harness.manual.finishBlink();
    harness.time.currentMs += 100;
    harness.controller.update(harness.time.currentMs, {});
    TEST_ASSERT_EQUAL(DeathController::State::Idle, harness.controller.state());
}

static void test_fortune_flow_without_preamble_prints_immediately(void) {
    TestHarness harness;
    harness.audio.addDirectory("/audio/welcome", {"/audio/welcome/hello.wav"});
    harness.audio.addDirectory("/audio/finger_prompt", {"/audio/finger_prompt/prompt.wav"});
    harness.audio.addDirectory("/audio/finger_snap", {"/audio/finger_snap/snap.wav"});
    harness.audio.addDirectory("/audio/no_finger", {"/audio/no_finger/nope.wav"});
    harness.audio.addDirectory("/audio/fortune_told", {"/audio/fortune_told/done.wav"});
    harness.fortune.nextFortuneText = "Instant fortune.";

    auto config = harness.defaultConfig();
    harness.controller.initialize(config);
    harness.controller.clearActions();
    harness.random.forcedValue = static_cast<int>(config.snapDelayMinMs);

    harness.time.currentMs = 5000;
    harness.controller.handleUartCommand(UARTCommand::FAR_MOTION_TRIGGER);
    TEST_ASSERT_EQUAL(DeathController::State::PlayWelcome, harness.controller.state());
    harness.controller.clearActions();

    harness.controller.handleAudioFinished("/audio/welcome/hello.wav");
    TEST_ASSERT_EQUAL(DeathController::State::WaitForNear, harness.controller.state());
    harness.controller.clearActions();

    harness.time.currentMs += 2500;
    harness.controller.handleUartCommand(UARTCommand::NEAR_MOTION_TRIGGER);
    TEST_ASSERT_EQUAL(DeathController::State::PlayFingerPrompt, harness.controller.state());
    harness.controller.clearActions();

    harness.controller.handleAudioFinished("/audio/finger_prompt/prompt.wav");
    TEST_ASSERT_EQUAL(DeathController::State::MouthOpenWaitFinger, harness.controller.state());
    harness.controller.clearActions();

    DeathController::FingerReadout readout{};
    readout.detected = true;
    readout.stable = true;
    harness.time.currentMs += 200;
    harness.controller.update(harness.time.currentMs, readout);
    harness.controller.clearActions();
    TEST_ASSERT_EQUAL(DeathController::State::FingerDetected, harness.controller.state());

    harness.time.currentMs += config.snapDelayMinMs;
    harness.controller.update(harness.time.currentMs, readout);
    harness.controller.clearActions();
    TEST_ASSERT_EQUAL(DeathController::State::SnapWithFinger, harness.controller.state());

    harness.controller.handleAudioFinished("/audio/finger_snap/snap.wav");
    const auto &actions = harness.controller.pendingActions();
    TEST_ASSERT_EQUAL(DeathController::State::FortuneDone, harness.controller.state());
    TEST_ASSERT_TRUE_MESSAGE(actions.queueFortunePrint, "Fortune should be queued when preamble audio missing");
    TEST_ASSERT_EQUAL_STRING("Instant fortune.", actions.fortuneText.c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_far_trigger_queues_welcome_audio);
    RUN_TEST(test_near_trigger_requires_wait_for_near);
    RUN_TEST(test_printer_not_ready_skips_queue);
    RUN_TEST(test_printer_ready_queues_print);
    RUN_TEST(test_finger_timeout_transitions_to_no_finger);
    RUN_TEST(test_cooldown_transitions_to_idle_after_timeout);
    RUN_TEST(test_manual_calibration_trigger_after_hold);
    RUN_TEST(test_fortune_flow_without_preamble_prints_immediately);
    return UNITY_END();
}
