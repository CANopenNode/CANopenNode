#pragma once

#include "invert_value.h"
#include "stdint.h"

template <typename T>
struct DigitalOutputModule {
    T val;
    uint16_t od_index;
    uint8_t od_subindex;
    uint8_t length;
    uint8_t channel;
    bool error_mode;
    bool error_value;
    bool polarity;
    bool filter_mask;
};

template <typename T>
T getDigitalOutputFiltered(DigitalOutputModule<T> module, const bool failure = false) {
    T val = module.val;

    if (failure) {
        if (module.error_mode) {
            val = module.error_value;
        }
    } else {
        if (module.polarity) {
            val = invertValue(val);
        }

        if (!module.filter_mask) {
            val = 0;
        }
    }
    return val;
}