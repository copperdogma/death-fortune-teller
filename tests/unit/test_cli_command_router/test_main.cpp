#include <Arduino.h>
#include <unity.h>

#include "cli_command_router.h"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "finger_sensor_stub.h"
#include "servo_controller_stub.h"
#include "thermal_printer_stub.h"

namespace {

class CapturePrinter : public CliCommandRouter::IPrinter {
public:
    void print(const String &value) override {
        buffer += value;
        transcript += value.toStdString();
    }

    void println(const String &value) override {
        buffer += value;
        lines.push_back(buffer);
        transcript += buffer.toStdString();
        transcript += "\n";
        buffer = "";
    }

    void println() override {
        lines.push_back(buffer);
        transcript += buffer.toStdString();
        transcript += "\n";
        buffer = "";
    }

    void printf(const char *fmt, ...) override {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        String str(buf);
        buffer += str;
        transcript += str.toStdString();
    }

    std::vector<String> lines;
    std::string transcript;

private:
    String buffer;
};

struct RouterFixture {
    CapturePrinter printer;
    FingerSensor sensor;
    ServoController servo;
    ThermalPrinter printerDevice;
    unsigned long stableMs = 120;
    Print fingerPrinter;
    int servoPinValue = 23;
    bool configPrinted = false;
    bool sdPrinted = false;
    bool fallbackCalled = false;
    String lastFallbackCommand;
    CliCommandRouter router;

    RouterFixture(bool withFallback = false)
        : router(buildDeps(withFallback)) {
        servo.setInitialState(10, 0, 80, 1500, 1600);
        printerDevice.setReady(true);
    }

private:
    CliCommandRouter::Dependencies buildDeps(bool withFallback) {
        CliCommandRouter::Dependencies deps;
        deps.printer = &printer;
        deps.fingerSensor = &sensor;
        deps.fingerStableDurationMs = &stableMs;
        deps.fingerStatusPrinter = &fingerPrinter;
        deps.servoController = &servo;
        deps.servoPin = &servoPinValue;
        deps.thermalPrinter = &printerDevice;
        deps.configPrinter = [&]() { configPrinted = true; };
        deps.sdInfoPrinter = [&](CliCommandRouter::IPrinter &out) {
            sdPrinted = true;
            out.println("\n=== SD SUMMARY ===");
        };
        deps.thermalPrinter = &printerDevice;
        if (withFallback) {
            deps.legacyHandler = [&](String cmd) {
                fallbackCalled = true;
                lastFallbackCommand = cmd;
            };
        }
        return deps;
    }
};

}  // namespace

static void test_help_command_outputs_overview() {
    RouterFixture fx;
    fx.router.handleCommand("help");

    TEST_ASSERT_GREATER_THAN(0, static_cast<int>(fx.printer.lines.size()));
    TEST_ASSERT_EQUAL_STRING("\n=== CLI COMMANDS ===", fx.printer.lines[0].c_str());
    bool hasHelp = false;
    bool hasFinger = false;
    for (const auto &line : fx.printer.lines) {
        auto text = line.toStdString();
        if (text.find("help | ?") != std::string::npos) {
            hasHelp = true;
        }
        if (text.find("fhelp | f?") != std::string::npos) {
            hasFinger = true;
        }
    }
    TEST_ASSERT_TRUE(hasHelp);
    TEST_ASSERT_TRUE(hasFinger);
}

static void test_fallback_invoked_for_unknown_command() {
    RouterFixture fx(true);
    fx.router.handleCommand("servo_magic");
    TEST_ASSERT_TRUE(fx.fallbackCalled);
    TEST_ASSERT_EQUAL_STRING("servo_magic", fx.lastFallbackCommand.c_str());
}

static void test_calibration_command_calls_sensor() {
    RouterFixture fx;
    fx.router.handleCommand("cal");
    TEST_ASSERT_TRUE(fx.sensor.calibrated);
}

static void test_fsens_reports_current_values() {
    RouterFixture fx;
    fx.sensor.sensitivity = 0.42f;
    fx.sensor.noiseNormalized = 0.25f;
    fx.router.handleCommand("fsens");
    TEST_ASSERT_TRUE(fx.printer.transcript.find("0.420") != std::string::npos);
}

static void test_fsens_sets_new_value() {
    RouterFixture fx;
    fx.router.handleCommand("fsens 0.2");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.2f, fx.sensor.sensitivity);
}

static void test_fthresh_sets_threshold() {
    RouterFixture fx;
    fx.router.handleCommand("fthresh 0.015");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.015f, fx.sensor.threshold);
}

static void test_fdebounce_updates_duration() {
    RouterFixture fx;
    fx.router.handleCommand("fdebounce 350");
    TEST_ASSERT_EQUAL_UINT32(350, fx.sensor.stableDurationMs);
    TEST_ASSERT_EQUAL_UINT32(350, fx.stableMs);
}

static void test_finterval_sets_stream_interval() {
    RouterFixture fx;
    fx.router.handleCommand("finterval 750");
    TEST_ASSERT_EQUAL_UINT32(750, fx.sensor.streamIntervalMs);
}

static void test_fon_and_foff_toggle_stream() {
    RouterFixture fx;
    fx.router.handleCommand("fon");
    TEST_ASSERT_TRUE(fx.sensor.streamEnabled);
    fx.router.handleCommand("foff");
    TEST_ASSERT_FALSE(fx.sensor.streamEnabled);
}

static void test_fcycles_sets_touch_cycles() {
    RouterFixture fx;
    fx.router.handleCommand("fcycles 0x10 0x20");
    TEST_ASSERT_EQUAL_UINT16(0x10, fx.sensor.touchCyclesInit);
    TEST_ASSERT_EQUAL_UINT16(0x20, fx.sensor.touchCyclesMeasure);
}

static void test_falpha_sets_filter_alpha() {
    RouterFixture fx;
    fx.router.handleCommand("falpha 0.45");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.45f, fx.sensor.filterAlpha);
}

static void test_fdrift_sets_baseline_drift() {
    RouterFixture fx;
    fx.router.handleCommand("fdrift 0.02");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.02f, fx.sensor.baselineDrift);
}

static void test_fmultisample_sets_count() {
    RouterFixture fx;
    fx.router.handleCommand("fmultisample 5");
    TEST_ASSERT_EQUAL_UINT8(5, fx.sensor.multisampleCount);
}

static void test_fstatus_invokes_sensor_status() {
    RouterFixture fx;
    fx.router.handleCommand("fstatus");
    TEST_ASSERT_TRUE(fx.sensor.statusPrinted);
}

static void test_fsettings_invokes_sensor_settings() {
    RouterFixture fx;
    fx.router.handleCommand("fsettings");
    TEST_ASSERT_TRUE(fx.sensor.settingsPrinted);
}

static void test_config_command_invokes_printer() {
    RouterFixture fx;
    fx.configPrinted = false;
    fx.router.handleCommand("config");
    TEST_ASSERT_TRUE(fx.configPrinted);
}

static void test_settings_alias_invokes_printer() {
    RouterFixture fx;
    fx.configPrinted = false;
    fx.router.handleCommand("settings");
    TEST_ASSERT_TRUE(fx.configPrinted);
}

static void test_sd_command_uses_provider() {
    RouterFixture fx;
    fx.sdPrinted = false;
    fx.router.handleCommand("sd");
    TEST_ASSERT_TRUE(fx.sdPrinted);
}

static void test_sdcard_alias_uses_provider() {
    RouterFixture fx;
    fx.sdPrinted = false;
    fx.router.handleCommand("sdcard");
    TEST_ASSERT_TRUE(fx.sdPrinted);
}

static void test_ptest_runs_when_ready() {
    RouterFixture fx;
    fx.printerDevice.setReady(true);
    fx.printerDevice.setTestPageResult(true);
    fx.router.handleCommand("ptest");
    TEST_ASSERT_TRUE(fx.printerDevice.printTestPageCalled);
    TEST_ASSERT_TRUE(fx.printer.transcript.find("Printer self-test initiated") != std::string::npos);
}

static void test_ptest_reports_when_not_ready() {
    RouterFixture fx;
    fx.printerDevice.setReady(false);
    fx.router.handleCommand("ptest");
    TEST_ASSERT_TRUE(fx.printer.transcript.find("not ready") != std::string::npos);
}

static void test_ptest_failure_path() {
    RouterFixture fx;
    fx.printerDevice.setReady(true);
    fx.printerDevice.setTestPageResult(false);
    fx.router.handleCommand("ptest");
    TEST_ASSERT_TRUE(fx.printer.transcript.find("Failed to start") != std::string::npos);
}

static void test_ptest_without_printer_reports_error() {
    CapturePrinter printer;
    CliCommandRouter::Dependencies deps;
    deps.printer = &printer;
    CliCommandRouter router(deps);
    router.handleCommand("ptest");
    TEST_ASSERT_TRUE(printer.transcript.find("Thermal printer not initialized") != std::string::npos);
}

static void test_servo_init_runs_when_ready() {
    RouterFixture fx;
    fx.servo.setInitialState(20, 0, 80, 1400, 1600);
    fx.router.handleCommand("sinit");
    TEST_ASSERT_EQUAL(1, fx.servo.reattachCalls);
    TEST_ASSERT_TRUE(fx.printer.transcript.find("Servo sweep complete") != std::string::npos);
}

static void test_servo_init_errors_when_uninitialized() {
    RouterFixture fx;
    fx.servo.setInitialState(-1, 0, 80, 1400, 1600);
    fx.router.handleCommand("sinit");
    TEST_ASSERT_TRUE(fx.printer.transcript.find("Servo not initialized") != std::string::npos);
}

static void test_smin_moves_to_min() {
    RouterFixture fx;
    fx.servo.setInitialState(40, 0, 80, 1400, 1600);
    fx.router.handleCommand("smin");
    TEST_ASSERT_EQUAL(0, fx.servo.lastSmoothMoveTarget);
}

static void test_smin_adjusts_microseconds() {
    RouterFixture fx;
    fx.servo.setInitialState(10, 0, 80, 1500, 1600);
    fx.router.handleCommand("smin +");
    TEST_ASSERT_EQUAL(1600, fx.servo.getMinMicroseconds());
    fx.router.handleCommand("smin -");
    TEST_ASSERT_EQUAL(1500, fx.servo.getMinMicroseconds());
}

static void test_smax_moves_to_max() {
    RouterFixture fx;
    fx.servo.setInitialState(10, 0, 80, 1400, 1600);
    fx.router.handleCommand("smax");
    TEST_ASSERT_EQUAL(80, fx.servo.lastSmoothMoveTarget);
}

static void test_smic_sets_pulse_width() {
    RouterFixture fx;
    fx.router.handleCommand("smic 1800");
    TEST_ASSERT_EQUAL(1800, fx.servo.lastWrittenMicros);
}

static void test_sdeg_sets_position() {
    RouterFixture fx;
    fx.router.handleCommand("sdeg 45");
    TEST_ASSERT_EQUAL(45, fx.servo.lastSetPosition);
}

static void test_srev_toggles_direction() {
    RouterFixture fx;
    fx.router.handleCommand("srev");
    TEST_ASSERT_TRUE(fx.servo.isReversed());
    fx.router.handleCommand("srev");
    TEST_ASSERT_FALSE(fx.servo.isReversed());
}

static void test_scfg_prints_configuration() {
    RouterFixture fx;
    fx.router.handleCommand("scfg");
    TEST_ASSERT_TRUE(fx.printer.transcript.find("Servo Configuration") != std::string::npos);
    TEST_ASSERT_TRUE(fx.printer.transcript.find("Pin: 23") != std::string::npos);
}

static void test_servo_commands_without_controller() {
    CapturePrinter printer;
    CliCommandRouter::Dependencies deps;
    deps.printer = &printer;
    CliCommandRouter router(deps);
    router.handleCommand("sinit");
    TEST_ASSERT_TRUE(printer.transcript.find("Servo controller not available") != std::string::npos);
}

static void test_missing_sensor_reports_error() {
    CapturePrinter printer;
    unsigned long stable = 0;
    Print statusPrint;
    CliCommandRouter::Dependencies deps;
    deps.printer = &printer;
    deps.fingerSensor = nullptr;
    deps.fingerStableDurationMs = &stable;
    deps.fingerStatusPrinter = &statusPrint;
    CliCommandRouter router(deps);

    router.handleCommand("fon");

    TEST_ASSERT_TRUE(printer.transcript.find("Finger sensor not initialized") != std::string::npos);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_help_command_outputs_overview);
    RUN_TEST(test_fallback_invoked_for_unknown_command);
    RUN_TEST(test_calibration_command_calls_sensor);
    RUN_TEST(test_fsens_reports_current_values);
    RUN_TEST(test_fsens_sets_new_value);
    RUN_TEST(test_fthresh_sets_threshold);
    RUN_TEST(test_fdebounce_updates_duration);
    RUN_TEST(test_finterval_sets_stream_interval);
    RUN_TEST(test_fon_and_foff_toggle_stream);
    RUN_TEST(test_fcycles_sets_touch_cycles);
    RUN_TEST(test_falpha_sets_filter_alpha);
    RUN_TEST(test_fdrift_sets_baseline_drift);
    RUN_TEST(test_fmultisample_sets_count);
    RUN_TEST(test_fstatus_invokes_sensor_status);
    RUN_TEST(test_fsettings_invokes_sensor_settings);
    RUN_TEST(test_config_command_invokes_printer);
    RUN_TEST(test_settings_alias_invokes_printer);
    RUN_TEST(test_sd_command_uses_provider);
    RUN_TEST(test_sdcard_alias_uses_provider);
    RUN_TEST(test_ptest_runs_when_ready);
    RUN_TEST(test_ptest_reports_when_not_ready);
    RUN_TEST(test_ptest_failure_path);
    RUN_TEST(test_ptest_without_printer_reports_error);
    RUN_TEST(test_servo_init_runs_when_ready);
    RUN_TEST(test_servo_init_errors_when_uninitialized);
    RUN_TEST(test_smin_moves_to_min);
    RUN_TEST(test_smin_adjusts_microseconds);
    RUN_TEST(test_smax_moves_to_max);
    RUN_TEST(test_smic_sets_pulse_width);
    RUN_TEST(test_sdeg_sets_position);
    RUN_TEST(test_srev_toggles_direction);
    RUN_TEST(test_scfg_prints_configuration);
    RUN_TEST(test_servo_commands_without_controller);
    RUN_TEST(test_missing_sensor_reports_error);
    return UNITY_END();
}
