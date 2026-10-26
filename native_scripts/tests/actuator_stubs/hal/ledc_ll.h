#pragma once
#include <cstdint>
struct actuator_test_ledc_t {
    struct Group {
        struct Channel { struct Conf { volatile uint32_t duty_start; } conf1; } channel[3];
    } channel_group[1];
};
extern actuator_test_ledc_t actuator_test_ledc;
#define LEDC_LL_GET_HW() (&actuator_test_ledc)
