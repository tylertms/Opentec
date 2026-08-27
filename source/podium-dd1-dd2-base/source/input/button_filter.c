#include "input/button_filter.h"

#include <stdint.h>

void wheel_button_filter_update(WheelButtonFilter *filter,
                                const uint8_t input[WHEEL_BUTTON_FILTER_BYTES],
                                uint8_t output[WHEEL_BUTTON_FILTER_BYTES]) {
    for (uint8_t index = 0; index < WHEEL_BUTTON_FILTER_BYTES; index++) {
        filter->history[filter->next_sample][index] = input[index];
        output[index] =
            filter->history[0][index] & filter->history[1][index] & filter->history[2][index];
    }

    filter->next_sample++;
    if (filter->next_sample == WHEEL_BUTTON_FILTER_SAMPLES) {
        filter->next_sample = 0;
    }
}
