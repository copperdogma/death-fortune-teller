#include "cli_command_router.h"

#include <cstdarg>
#include <vector>
#include <cstdlib>

#include "config_manager.h"

#ifndef UNIT_TEST
#include "finger_sensor.h"
#include "servo_controller.h"
#include "thermal_printer.h"
#else
#include "../tests/support/finger_sensor_stub.h"
#include "../tests/support/servo_controller_stub.h"
#include "../tests/support/thermal_printer_stub.h"
#endif

namespace {

class NullPrinter : public CliCommandRouter::IPrinter {
public:
    void print(const String &) override {}
    void println(const String &) override {}
    void println() override {}
    void printf(const char *, ...) override {}
};

}  // namespace

CliCommandRouter::CliCommandRouter(const Dependencies &deps)
    : m_deps(deps) {
    if (!m_deps.printer) {
        static NullPrinter nullPrinter;
        m_deps.printer = &nullPrinter;
    }
}

void CliCommandRouter::handleCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "help" || cmd == "?") {
        printHelp();
        return;
    }

    if (cmd == "fhelp" || cmd == "f?") {
        printFingerHelp();
        return;
    }

    if (handleFingerCommand(cmd)) {
        return;
    }

    if (handleServoCommand(cmd)) {
        return;
    }

    if (cmd == "config" || cmd == "settings") {
        if (m_deps.configPrinter) {
            m_deps.configPrinter();
        } else {
            m_deps.printer->println(">>> ERROR: Config printer unavailable\n");
        }
        return;
    }

    if (cmd == "sd" || cmd == "sdcard") {
        printSDCardInfo();
        return;
    }

    if (cmd == "ptest") {
        auto *printerDevice = thermalPrinter();
        if (!printerDevice) {
            m_deps.printer->println(">>> ERROR: Thermal printer not initialized\n");
            return;
        }
        if (!printerDevice->isReady()) {
            m_deps.printer->println(">>> ERROR: Thermal printer not ready (check power/paper)\n");
            return;
        }
        if (printerDevice->printTestPage()) {
            m_deps.printer->println(">>> Printer self-test initiated. Check the diagnostic printout.\n");
        } else {
            m_deps.printer->println(">>> ERROR: Failed to start printer self-test\n");
        }
        return;
    }

    if (cmd == "pstatus") {
        auto *printerDevice = thermalPrinter();
        if (!printerDevice) {
            m_deps.printer->println(">>> ERROR: Thermal printer not initialized\n");
            return;
        }
        m_deps.printer->println("\n=== PRINTER STATUS ===");
        m_deps.printer->printf("Ready:      %s\n", printerDevice->isReady() ? "YES" : "NO");
        m_deps.printer->printf("Printing:   %s\n", printerDevice->isPrinting() ? "YES" : "NO");
        m_deps.printer->printf("Error flag: %s\n\n", printerDevice->hasError() ? "YES" : "NO");
        return;
    }

    if (m_deps.legacyHandler) {
        m_deps.legacyHandler(cmd);
        return;
    }

    if (cmd.length() > 0) {
        m_deps.printer->print(">>> Unknown command: ");
        m_deps.printer->println(cmd + ". Type 'help' for commands.\n");
    }
}

void CliCommandRouter::printHelp() {
    m_deps.printer->println("\n=== CLI COMMANDS ===");
    m_deps.printer->println("help | ?           - Show this help message");
    m_deps.printer->println("fhelp | f?        - Finger sensor help");
    m_deps.printer->println();
}

void CliCommandRouter::printFingerHelp() {
    m_deps.printer->println("\n=== FINGER SENSOR COMMANDS ===");
    m_deps.printer->println("cal / fcal         - Run finger sensor calibration");
    m_deps.printer->println("fsens [value]      - Get/Set sensitivity margin");
    m_deps.printer->println("fthresh [value]    - Set minimum threshold ratio");
    m_deps.printer->println("fdebounce <ms>     - Set stable detection duration");
    m_deps.printer->println("finterval <ms>     - Set streaming interval");
    m_deps.printer->println("fon / foff         - Enable/disable streaming");
    m_deps.printer->println("fcycles <init> <meas> - Set touch cycles");
    m_deps.printer->println("falpha <0-1>       - Set smoothing alpha");
    m_deps.printer->println("fdrift <0-0.1>     - Set baseline drift factor");
    m_deps.printer->println("fmultisample <N>   - Set sample averaging count");
    m_deps.printer->println();
}

void CliCommandRouter::printSDCardInfo() {
    if (m_deps.sdInfoPrinter) {
        m_deps.sdInfoPrinter(*m_deps.printer);
    } else {
        m_deps.printer->println("\n=== SD CARD INFO ===");
        m_deps.printer->println("Diagnostics unavailable (no provider)\n");
    }
}

bool CliCommandRouter::handleFingerCommand(String &cmd) {
    auto *sensor = fingerSensor();
    const auto missingSensor = [&]() {
        m_deps.printer->println(">>> ERROR: Finger sensor not initialized\n");
        return true;
    };

    if (cmd == "cal" || cmd == "calibrate" || cmd == "fcal" || cmd == "fcalibrate") {
        if (!sensor) {
            return missingSensor();
        }
        m_deps.printer->println("\n>>> Calibrating finger sensor...");
        m_deps.printer->println(">>> REMOVE YOUR FINGER NOW!");
        sensor->calibrate();
        m_deps.printer->println(">>> Calibration running (watch logs for completion)...\n");
        return true;
    }

    if (cmd == "fsens" || cmd.indexOf("fsens ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        String valuePart = cmd.length() > 5 ? cmd.substring(5) : "";
        valuePart.trim();
        if (valuePart.length() == 0) {
            float sens = sensor->getSensitivity();
            float noise = sensor->getNoiseNormalized() * 100.0f;
            m_deps.printer->printf(">>> Current sensitivity margin: %.3f (noise delta %.3f%%)\n\n", sens, noise);
            return true;
        }
        float val = valuePart.toFloat();
        if (val < 0.0f || val > 1.0f) {
            m_deps.printer->println(">>> ERROR: Sensitivity must be between 0.0 and 1.0\n");
            return true;
        }
        if (sensor->setSensitivity(val)) {
            float threshold = sensor->getThresholdRatio() * 100.0f;
            m_deps.printer->printf(">>> Sensitivity margin set to %.3f (effective threshold %.3f%%)\n\n", val, threshold);
        } else {
            m_deps.printer->println(">>> ERROR: Failed to set sensitivity\n");
        }
        return true;
    }

    if (cmd == "thresh" || cmd.indexOf("thresh ") == 0 || cmd == "fthresh" || cmd.indexOf("fthresh ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        String valuePart;
        if (cmd.indexOf("fthresh") == 0) {
            valuePart = cmd.substring(strlen("fthresh"));
        } else {
            valuePart = cmd.substring(strlen("thresh"));
        }
        valuePart.trim();
        if (valuePart.length() == 0) {
            m_deps.printer->println(">>> ERROR: Missing value. Use fthresh <value>\n");
            return true;
        }
        float val = valuePart.toFloat();
        if (val <= 0.0f || val > 1.0f) {
            m_deps.printer->println(">>> ERROR: Threshold must be between 0 and 1 (e.g., 0.002)\n");
            return true;
        }
        if (sensor->setThresholdRatio(val)) {
            m_deps.printer->printf(">>> Minimum threshold clamp set to %.3f%%\n\n", val * 100.0f);
        } else {
            m_deps.printer->println(">>> ERROR: Threshold out of supported range (0.0001 - 1.0)\n");
        }
        return true;
    }

    if (cmd.indexOf("fdebounce ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        unsigned long val = cmd.substring(strlen("fdebounce ")).toInt();
        if (sensor->setStableDurationMs(val)) {
            if (auto durationPtr = fingerStableDuration()) {
                *durationPtr = val;
            }
            m_deps.printer->printf(">>> Stable detection duration set to %lu ms\n\n", val);
        } else {
            m_deps.printer->println(">>> ERROR: Duration must be between 30 and 1000 ms\n");
        }
        return true;
    }

    if (cmd.indexOf("finterval ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        unsigned long val = cmd.substring(strlen("finterval ")).toInt();
        if (sensor->setStreamIntervalMs(val)) {
            m_deps.printer->printf(">>> Stream interval set to %lu ms\n\n", val);
        } else {
            m_deps.printer->println(">>> ERROR: Interval must be between 100 and 10000 ms\n");
        }
        return true;
    }

    if (cmd == "fon") {
        if (!sensor) {
            return missingSensor();
        }
        sensor->setStreamEnabled(true);
        m_deps.printer->println(">>> Finger sensor stream enabled\n");
        return true;
    }

    if (cmd == "foff") {
        if (!sensor) {
            return missingSensor();
        }
        sensor->setStreamEnabled(false);
        m_deps.printer->println(">>> Finger sensor stream disabled\n");
        return true;
    }

    if (cmd == "fstatus") {
        if (!sensor) {
            return missingSensor();
        }
        if (Print *out = fingerStatusPrinter()) {
            sensor->printStatus(*out);
            m_deps.printer->println();
        } else {
            m_deps.printer->println(">>> ERROR: Status printer unavailable\n");
        }
        return true;
    }

    if (cmd == "fsettings") {
        if (!sensor) {
            return missingSensor();
        }
        if (Print *out = fingerStatusPrinter()) {
            sensor->printSettings(*out);
            m_deps.printer->println();
        } else {
            m_deps.printer->println(">>> ERROR: Status printer unavailable\n");
        }
        return true;
    }

    if (cmd.indexOf("fcycles ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        String args = cmd.substring(strlen("fcycles"));
        args.trim();
        int spaceIdx = args.indexOf(' ');
        if (spaceIdx <= 0) {
            m_deps.printer->println(">>> ERROR: Usage fcycles <initial> <measure>\n");
            return true;
        }
        String initStr = args.substring(0, spaceIdx);
        String measStr = args.substring(spaceIdx + 1);
        uint16_t initVal = static_cast<uint16_t>(strtol(initStr.c_str(), nullptr, 0));
        uint16_t measVal = static_cast<uint16_t>(strtol(measStr.c_str(), nullptr, 0));
        if (sensor->setTouchCycles(initVal, measVal)) {
            m_deps.printer->printf(">>> touchSetCycles updated to init=0x%X measure=0x%X\n\n", initVal, measVal);
        } else {
            m_deps.printer->println(">>> ERROR: Invalid cycle values (must be >0)\n");
        }
        return true;
    }

    if (cmd.indexOf("falpha ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        float val = cmd.substring(strlen("falpha ")).toFloat();
        if (sensor->setFilterAlpha(val)) {
            m_deps.printer->printf(">>> Filter alpha set to %.4f\n\n", val);
        } else {
            m_deps.printer->println(">>> ERROR: Alpha must be within 0.0 - 1.0\n");
        }
        return true;
    }

    if (cmd.indexOf("fdrift ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        float val = cmd.substring(strlen("fdrift ")).toFloat();
        if (sensor->setBaselineDrift(val)) {
            m_deps.printer->printf(">>> Baseline drift set to %.6f\n\n", val);
        } else {
            m_deps.printer->println(">>> ERROR: Drift must be within 0.0 - 0.1\n");
        }
        return true;
    }

    if (cmd.indexOf("fmultisample ") == 0) {
        if (!sensor) {
            return missingSensor();
        }
        int val = cmd.substring(strlen("fmultisample ")).toInt();
        if (sensor->setMultisampleCount(static_cast<uint8_t>(val))) {
            m_deps.printer->printf(">>> Multisample count set to %d\n\n", val);
        } else {
            m_deps.printer->println(">>> ERROR: Count must be >= 1\n");
        }
        return true;
    }

    return false;
}

bool CliCommandRouter::handleServoCommand(String &cmd) {
    auto *servo = servoController();
    const auto controllerMissing = [&]() {
        m_deps.printer->println(">>> ERROR: Servo controller not available\n");
        return true;
    };
    const auto servoNotInitialized = [&]() {
        m_deps.printer->println(">>> ERROR: Servo not initialized\n");
        return true;
    };

    if (cmd == "sinit") {
        if (!servo) {
            return controllerMissing();
        }
        m_deps.printer->println("\n>>> Running servo initialization sweep...");
        if (servo->getPosition() >= 0) {
            int minUs = servo->getMinMicroseconds();
            int maxUs = servo->getMaxMicroseconds();
            int minDeg = servo->getMinDegrees();
            int maxDeg = servo->getMaxDegrees();
            m_deps.printer->printf(">>> Config: degrees %d-%d, microseconds %d-%d µs\n",
                                   minDeg, maxDeg, minUs, maxUs);
            servo->reattachWithConfigLimits();
            m_deps.printer->println(">>> Servo sweep complete!\n");
        } else {
            servoNotInitialized();
        }
        return true;
    }

    if (cmd.indexOf("smin") == 0) {
        if (!servo) {
            return controllerMissing();
        }
        String valuePart = cmd.substring(4);
        valuePart.trim();
        if (valuePart.length() == 0) {
            m_deps.printer->println("\n>>> Moving servo to MIN position...");
            if (servo->getPosition() >= 0) {
                int minDeg = servo->getMinDegrees();
                int minUs = servo->getMinMicroseconds();
                m_deps.printer->printf(">>> Moving to MIN: %d° (%d µs)\n", minDeg, minUs);
                servo->smoothMove(minDeg, 500);
                m_deps.printer->printf(">>> Servo moved to MIN: %d degrees (%d µs)\n\n", minDeg, minUs);
            } else {
                servoNotInitialized();
            }
        } else if (valuePart == "+") {
            int newMin = servo->getMinMicroseconds() + 100;
            servo->setMinMicroseconds(newMin);
            m_deps.printer->printf("\n>>> MIN increased to %d µs\n\n", servo->getMinMicroseconds());
        } else if (valuePart == "-") {
            int newMin = servo->getMinMicroseconds() - 100;
            servo->setMinMicroseconds(newMin);
            m_deps.printer->printf("\n>>> MIN decreased to %d µs\n\n", servo->getMinMicroseconds());
        } else if (valuePart.length() > 0) {
            int newMin = valuePart.toInt();
            if (newMin >= 500 && newMin <= 10000) {
                servo->setMinMicroseconds(newMin);
                m_deps.printer->printf("\n>>> MIN set to %d µs\n\n", servo->getMinMicroseconds());
            } else {
                m_deps.printer->println(">>> ERROR: Value must be 500-10000 µs\n");
            }
        }
        return true;
    }

    if (cmd.indexOf("smax") == 0) {
        if (!servo) {
            return controllerMissing();
        }
        String valuePart = cmd.substring(4);
        valuePart.trim();
        if (valuePart.length() == 0) {
            m_deps.printer->println("\n>>> Moving servo to MAX position...");
            if (servo->getPosition() >= 0) {
                int maxDeg = servo->getMaxDegrees();
                int maxUs = servo->getMaxMicroseconds();
                m_deps.printer->printf(">>> Moving to MAX: %d° (%d µs)\n", maxDeg, maxUs);
                servo->smoothMove(maxDeg, 500);
                m_deps.printer->printf(">>> Servo moved to MAX: %d degrees (%d µs)\n\n", maxDeg, maxUs);
            } else {
                servoNotInitialized();
            }
        } else if (valuePart == "+") {
            int newMax = servo->getMaxMicroseconds() + 100;
            servo->setMaxMicroseconds(newMax);
            m_deps.printer->printf("\n>>> MAX increased to %d µs\n\n", servo->getMaxMicroseconds());
        } else if (valuePart == "-") {
            int newMax = servo->getMaxMicroseconds() - 100;
            servo->setMaxMicroseconds(newMax);
            m_deps.printer->printf("\n>>> MAX decreased to %d µs\n\n", servo->getMaxMicroseconds());
        } else if (valuePart.length() > 0) {
            int newMax = valuePart.toInt();
            if (newMax >= 500 && newMax <= 10000) {
                servo->setMaxMicroseconds(newMax);
                m_deps.printer->printf("\n>>> MAX set to %d µs\n\n", servo->getMaxMicroseconds());
            } else {
                m_deps.printer->println(">>> ERROR: Value must be 500-10000 µs\n");
            }
        }
        return true;
    }

    if (cmd == "scfg") {
        if (!servo) {
            return controllerMissing();
        }
        m_deps.printer->println("\n>>> Servo Configuration:");
        if (m_deps.servoPin) {
            m_deps.printer->printf("  Pin: %d\n", *m_deps.servoPin);
        } else {
            m_deps.printer->println("  Pin: [unknown]");
        }
        m_deps.printer->printf("  Degree range: %d-%d\n", servo->getMinDegrees(), servo->getMaxDegrees());
        m_deps.printer->printf("  Pulse width range: %d-%d µs\n", servo->getMinMicroseconds(), servo->getMaxMicroseconds());
        m_deps.printer->printf("  Current MIN: %d µs | MAX: %d µs\n", servo->getMinMicroseconds(), servo->getMaxMicroseconds());
        m_deps.printer->printf("  Current position: %d degrees\n", servo->getPosition());
        m_deps.printer->printf("  Direction reversed: %s\n", servo->isReversed() ? "YES" : "NO");
        m_deps.printer->println();
        return true;
    }

    if (cmd.indexOf("smic ") == 0) {
        if (!servo) {
            return controllerMissing();
        }
        String valuePart = cmd.substring(strlen("smic "));
        valuePart.trim();
        if (valuePart.length() == 0) {
            m_deps.printer->println(">>> ERROR: Specify microseconds (500-2500)\n");
            return true;
        }
        int us = valuePart.toInt();
        if (us < 500 || us > 2500) {
            m_deps.printer->println(">>> ERROR: Pulse width must be 500-2500 µs\n");
        } else {
            servo->writeMicroseconds(us);
            m_deps.printer->printf(">>> Servo set to %d µs (pulse width)\n\n", us);
        }
        return true;
    }

    if (cmd.indexOf("sdeg ") == 0) {
        if (!servo) {
            return controllerMissing();
        }
        String valuePart = cmd.substring(strlen("sdeg "));
        valuePart.trim();
        if (valuePart.length() == 0) {
            m_deps.printer->println(">>> ERROR: Specify degrees (0-80)\n");
            return true;
        }
        int deg = valuePart.toInt();
        if (deg < 0 || deg > 80) {
            m_deps.printer->println(">>> ERROR: Degrees must be 0-80\n");
        } else {
            servo->setPosition(deg);
            m_deps.printer->printf(">>> Servo set to %d degrees\n\n", deg);
        }
        return true;
    }

    if (cmd == "srev") {
        if (!servo) {
            return controllerMissing();
        }
        bool wasReversed = servo->isReversed();
        servo->setReverseDirection(!wasReversed);
        m_deps.printer->printf("\n>>> Servo direction reversal %s\n",
                               !wasReversed ? "ENABLED" : "DISABLED");
        m_deps.printer->printf("    Current state: %s\n", servo->isReversed() ? "REVERSED" : "NORMAL");
        m_deps.printer->println();
        return true;
    }

    return false;
}

FingerSensor *CliCommandRouter::fingerSensor() const {
    return m_deps.fingerSensor;
}

unsigned long *CliCommandRouter::fingerStableDuration() const {
    return m_deps.fingerStableDurationMs;
}

Print *CliCommandRouter::fingerStatusPrinter() const {
    return m_deps.fingerStatusPrinter;
}

ServoController *CliCommandRouter::servoController() const {
    return m_deps.servoController;
}

int CliCommandRouter::servoPin() const {
    return m_deps.servoPin ? *m_deps.servoPin : -1;
}

ThermalPrinter *CliCommandRouter::thermalPrinter() const {
    return m_deps.thermalPrinter;
}
