#ifndef INFRA_ARDUINO_TIME_PROVIDER_H
#define INFRA_ARDUINO_TIME_PROVIDER_H

#include <Arduino.h>

#include "infra/time_provider.h"

namespace infra {

class ArduinoTimeProvider : public ITimeProvider {
public:
    uint32_t nowMillis() const override {
        return millis();
    }

    uint64_t nowMicros() const override {
        return micros();
    }
};

}  // namespace infra

#endif  // INFRA_ARDUINO_TIME_PROVIDER_H
