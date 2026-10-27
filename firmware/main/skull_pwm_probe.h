#pragma once
#include <cstdint>
#include "esp_err.h"

namespace skull::pwm_probe {
// Passive 8-second GPIO23 measurement. Call on the diagnostic/control task only.
// The probe never writes LEDC duty, disables its output, or changes jaw mode.
esp_err_t start(uint32_t commanded_duty);
void poll(uint32_t commanded_duty, bool idle);
bool active();
void cancel();
}
