#ifndef INFRA_ARDUINO_RANDOM_SOURCE_H
#define INFRA_ARDUINO_RANDOM_SOURCE_H

#include <Arduino.h>
#include "random_source.h"

namespace infra {

class ArduinoRandomSource : public IRandomSource {
public:
    int nextInt(int minInclusive, int maxExclusive) override;
};

} // namespace infra

#endif // INFRA_ARDUINO_RANDOM_SOURCE_H
