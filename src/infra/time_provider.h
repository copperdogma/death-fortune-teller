#ifndef INFRA_TIME_PROVIDER_H
#define INFRA_TIME_PROVIDER_H

#include <stdint.h>

namespace infra {

/**
 * Interface surface for time queries.
 * Lets host tests advance simulated time without touching Arduino's millis().
 */
class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;
    virtual uint32_t nowMillis() const = 0;
    virtual uint64_t nowMicros() const = 0;
};

}  // namespace infra

#endif  // INFRA_TIME_PROVIDER_H
