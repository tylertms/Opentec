#ifndef OPENTEC_BASE_INPUT_BUTTON_FILTER_H
#define OPENTEC_BASE_INPUT_BUTTON_FILTER_H

#include <stdint.h>

enum {
    WHEEL_BUTTON_FILTER_BYTES = 3,
    WHEEL_BUTTON_FILTER_SAMPLES = 3,
};

typedef struct {
    uint8_t history[WHEEL_BUTTON_FILTER_SAMPLES][WHEEL_BUTTON_FILTER_BYTES];
    uint8_t next_sample;
} WheelButtonFilter;

void wheel_button_filter_update(WheelButtonFilter *filter,
                                const uint8_t input[WHEEL_BUTTON_FILTER_BYTES],
                                uint8_t output[WHEEL_BUTTON_FILTER_BYTES]);

#endif
