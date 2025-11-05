#include <unity.h>
#include "config_manager.h"
#include "fake_filesystem.h"
#include "fake_log_sink.h"

static FakeLogSink g_logSink;

void setUp(void) {
    g_logSink.clear();
    ConfigManager::getInstance().setLogSink(&g_logSink);
}
void tearDown(void) {}

static void test_load_config_happy_path(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt",
               "speaker_name=Skull Speaker\n"
               "speaker_volume=75\n"
               "wifi_ssid=SkullWiFi\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL_STRING("Skull Speaker", config.getBluetoothSpeakerName().c_str());
    TEST_ASSERT_EQUAL(75, config.getSpeakerVolume());
    TEST_ASSERT_EQUAL_STRING("SkullWiFi", config.getWiFiSSID().c_str());
}

static void test_reload_clears_missing_keys(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt",
               "speaker_name=First\n"
               "wifi_ssid=InitialSSID\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL_STRING("InitialSSID", config.getWiFiSSID().c_str());

    fs.addFile("/config.txt",
               "speaker_name=Second\n"
               "speaker_volume=60\n");

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL_STRING("", config.getWiFiSSID().c_str());
    TEST_ASSERT_EQUAL(60, config.getSpeakerVolume());
}

static void test_defaults_align_with_spec(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt", "# empty config\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL_FLOAT(0.002f, config.getCapThreshold());
    TEST_ASSERT_EQUAL_STRING("/printer/fortunes_littlekid.json", config.getFortunesJson().c_str());
}

static void test_invalid_servo_values_fall_back(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt",
               "servo_us_min=1700\n"
               "servo_us_max=1600\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL(1400, config.getServoUSMin());
    TEST_ASSERT_EQUAL(1600, config.getServoUSMax());
}

static void test_logs_warning_for_invalid_speaker_volume(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt",
               "speaker_volume=500\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);
    g_logSink.clear();

    TEST_ASSERT_TRUE(config.loadConfig());

    bool foundWarn = false;
    for (const auto &entry : g_logSink.entries) {
        if (entry.level == infra::LogLevel::Warn && entry.message.find("Invalid speaker volume") != std::string::npos) {
            foundWarn = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(foundWarn, "Expected warning log for invalid speaker volume");
}

static void test_invalid_timing_defaults(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt",
               "finger_detect_ms=5\n"
               "finger_wait_ms=200\n"
               "snap_delay_min_ms=5000\n"
               "snap_delay_max_ms=1000\n"
               "cooldown_ms=1000\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL(120UL, config.getFingerDetectMs());
    TEST_ASSERT_EQUAL(6000UL, config.getFingerWaitMs());
    TEST_ASSERT_EQUAL(1000UL, config.getSnapDelayMinMs());
    TEST_ASSERT_EQUAL(3000UL, config.getSnapDelayMaxMs());
    TEST_ASSERT_EQUAL(12000UL, config.getCooldownMs());
}

static void test_invalid_led_pulse_defaults(void) {
    FakeFileSystem fs;
    fs.addFile("/config.txt",
               "mouth_led_bright=900\n"
               "mouth_led_pulse_min=-10\n"
               "mouth_led_pulse_max=400\n"
               "mouth_led_pulse_period_ms=150\n");

    ConfigManager &config = ConfigManager::getInstance();
    config.setFileSystem(&fs);

    TEST_ASSERT_TRUE(config.loadConfig());
    TEST_ASSERT_EQUAL_UINT8(255, config.getMouthLedBright());
    TEST_ASSERT_EQUAL_UINT8(0, config.getMouthLedPulseMin());
    TEST_ASSERT_EQUAL_UINT8(255, config.getMouthLedPulseMax());
    TEST_ASSERT_EQUAL(1500UL, config.getMouthLedPulsePeriodMs());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_load_config_happy_path);
    RUN_TEST(test_reload_clears_missing_keys);
    RUN_TEST(test_defaults_align_with_spec);
    RUN_TEST(test_invalid_servo_values_fall_back);
    RUN_TEST(test_logs_warning_for_invalid_speaker_volume);
    RUN_TEST(test_invalid_timing_defaults);
    RUN_TEST(test_invalid_led_pulse_defaults);
    return UNITY_END();
}
