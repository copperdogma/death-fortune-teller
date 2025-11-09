#ifndef CLI_COMMAND_ROUTER_H
#define CLI_COMMAND_ROUTER_H

#include <Arduino.h>
#include <functional>
#include <string>

class ConfigManager;
class FingerSensor;
class ServoController;
class ThermalPrinter;
class Print;

class CliCommandRouter {
public:
    class IPrinter {
    public:
        virtual ~IPrinter() = default;
        virtual void print(const String &value) = 0;
        virtual void println(const String &value) = 0;
        virtual void println() = 0;
        virtual void printf(const char *fmt, ...) = 0;
    };

    struct Dependencies {
        ConfigManager *config = nullptr;
        IPrinter *printer = nullptr;
        FingerSensor *fingerSensor = nullptr;
        unsigned long *fingerStableDurationMs = nullptr;
        Print *fingerStatusPrinter = nullptr;
        ServoController *servoController = nullptr;
        int *servoPin = nullptr;
        ThermalPrinter *thermalPrinter = nullptr;
        std::function<void()> configPrinter;
        std::function<void(IPrinter &)> sdInfoPrinter;
        std::function<void(String)> legacyHandler;
    };

    explicit CliCommandRouter(const Dependencies &deps);

    void handleCommand(String cmd);

private:
    void printHelp();
    void printFingerHelp();
    void printSDCardInfo();
    bool handleFingerCommand(String &cmd);
    bool handleServoCommand(String &cmd);
    FingerSensor *fingerSensor() const;
    unsigned long *fingerStableDuration() const;
    Print *fingerStatusPrinter() const;
    ServoController *servoController() const;
    int servoPin() const;
    ThermalPrinter *thermalPrinter() const;

    Dependencies m_deps;
};

#endif  // CLI_COMMAND_ROUTER_H
