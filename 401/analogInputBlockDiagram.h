#pragma once

#include "invertValue.h"
#include "stdint.h"

#include "debug.h"

template <typename T>
struct AnalogInputModule {
    T val;
    T last_val;

    uint16_t od_index;
    uint8_t od_subindex;
    uint8_t length;
    uint8_t channel;

    int32_t offset;
    int32_t pre_scaling;
    int32_t upper_limit;
    int32_t lower_limit;
    int32_t delta;
    int32_t negative_delta;
    int32_t positive_delta;
    bool interrupt_enable;
};

template <typename T>
T getAnalogInputFiltered(AnalogInputModule<T> module) {
    T filtered_val = module.val + module.offset;

    // Apply pre-scaling
    filtered_val = filtered_val * module.pre_scaling;

    const bool upper_limit_triggered = filtered_val >= module.upper_limit;
    const bool lower_limit_triggered = filtered_val < module.lower_limit;
    const int32_t delta = filtered_val - module.last_val;
    const bool delta_triggered = delta > module.delta || delta < module.negative_delta || delta > module.positive_delta;

    const bool condition = (upper_limit_triggered ^ lower_limit_triggered) && delta_triggered;

    return module.interrupt_enable && condition ? filtered_val : module.last_val;
}