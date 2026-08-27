#include "force_feedback/filter.h"

#include <stdint.h>

static const uint8_t force_filter_windows[] = {40, 35, 30, 25, 20, 15, 10, 7, 4, 2, 1};

static uint8_t select_window(uint8_t intensity) {
    if (intensity > 100 || intensity % 10 != 0) {
        return 1;
    }
    return force_filter_windows[intensity / 10];
}

void force_filter_configure(ForceFilter *filter, uint8_t intensity) {
    filter->total = 0;
    filter->window = select_window(intensity);
    filter->index = 0;

    for (uint8_t index = 0; index < filter->window; ++index) {
        filter->samples[index] = 0;
    }
}

int32_t force_filter_update(ForceFilter *filter, int32_t sample) {
    filter->total -= filter->samples[filter->index];
    filter->samples[filter->index] = sample;
    filter->total += sample;

    ++filter->index;
    if (filter->index == filter->window) {
        filter->index = 0;
    }

    return (int32_t)(filter->total / filter->window);
}
