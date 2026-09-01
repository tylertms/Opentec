#include "force_feedback/filter.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Sample windows selected for valid intensities from zero through 100 by ten. */
static const uint8_t force_filter_windows[] = {40, 35, 30, 25, 20, 15, 10, 7, 4, 2, 1};

/**
 * @brief Select the smoothing window for a tuning intensity.
 *
 * Maps intensities from 0 through 100 in steps of 10 to the configured sample counts. Other
 * values select the single-sample fallback.
 *
 * @param[in] intensity Smoothing intensity from the tuning profile.
 * @return Number of samples included in each average.
 */
static uint8_t select_window(uint8_t intensity) {
    if (intensity > 100 || intensity % 10 != 0) {
        return 1;
    }
    return force_filter_windows[intensity / 10];
}

void force_filter_configure(ForceFilter *filter, uint8_t intensity) {
    if (filter->configured && filter->previous_intensity == intensity) {
        return;
    }

    filter->previous_intensity = intensity;
    filter->window = select_window(intensity);
    filter->index = 0;

    for (uint8_t index = 0; index < filter->window; ++index) {
        filter->samples[index] = 0;
    }
    filter->configured = true;
}

int32_t force_filter_update(ForceFilter *filter, int32_t sample, uint32_t now_ms) {
    if (filter->deadline_ms > now_ms) {
        return filter->output;
    }

    filter->deadline_ms = now_ms + 1;
    uint8_t next_index = (uint8_t)(filter->index + 1);
    if (filter->index >= filter->window) {
        next_index = 0;
    }
    filter->index = next_index;
    filter->samples[next_index] = sample;

    int64_t total = 0;
    for (uint8_t index = 0; index < filter->window; ++index) {
        total += filter->samples[index];
    }
    filter->output = (int32_t)(total / filter->window);

    return filter->output;
}
