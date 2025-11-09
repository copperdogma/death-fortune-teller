#ifndef UNIT_TEST

#include <Arduino.h>
#include <utility>

#include "app_controller.h"
#include "infra/arduino_random_source.h"
#include "infra/arduino_time_provider.h"
#include "infra/log_sink.h"

namespace {

infra::ArduinoTimeProvider g_timeProvider;
infra::ArduinoRandomSource g_randomSource;

constexpr AppController::HardwarePins kHardwarePins{
    .eyeLed = 32,
    .mouthLed = 33,
    .servo = 23,
    .fingerSensor = 4,
    .printerTx = 18,
    .printerRx = 19,
    .uartMatterTx = 21,
    .uartMatterRx = 22,
};

AppController::ModuleOptions moduleOptions = AppController::ModuleOptions::DefaultsFromBuildFlags();
AppController::ModuleProviders moduleProviders{};

AppController g_appController(kHardwarePins,
                               g_timeProvider,
                               g_randomSource,
                               infra::getLogSink(),
                               std::move(moduleOptions),
                               std::move(moduleProviders));

}  // namespace

void setup() {
    g_appController.setup();
}

void loop() {
    g_appController.loop();
}

#endif  // UNIT_TEST
