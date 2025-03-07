#pragma once

#include "invertValue.h"
#include "stdint.h"

template <typename T>
struct DigitalInputModule {
    T val;
    uint16_t od_index;
    uint8_t od_subindex;
    uint8_t length;
    uint8_t channel;
    bool polarity;
    bool filter_constant;
    bool any_change;
    bool high_to_low;
    bool low_to_high;
    bool interrupt_enable;
};

template <typename T>
T getDigitalInputFiltered(DigitalInputModule<T> module) {
    T val = module.val;

    if (module.filter_constant) {
        val = 0;
    }

    if (module.polarity) {
        val = invertValue(val);
    }

    if (!module.interrupt_enable) {
        val = 0;
    }

    return val;
}