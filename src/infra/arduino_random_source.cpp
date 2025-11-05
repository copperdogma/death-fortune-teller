#include "arduino_random_source.h"

namespace infra {

int ArduinoRandomSource::nextInt(int minInclusive, int maxExclusive) {
    if (maxExclusive <= minInclusive) {
        return minInclusive;
    }
    return static_cast<int>(random(minInclusive, maxExclusive));
}

} // namespace infra
