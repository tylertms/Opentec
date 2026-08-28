#ifndef OPENTEC_BASE_FORCE_FEEDBACK_FILTER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_FILTER_H

#include <stdbool.h>
#include <stdint.h>

enum {
    FORCE_FILTER_MAXIMUM_WINDOW = 40,
    FORCE_FILTER_SAMPLE_CAPACITY = FORCE_FILTER_MAXIMUM_WINDOW + 1,
};

typedef struct {
    int32_t samples[FORCE_FILTER_SAMPLE_CAPACITY];
    int32_t output;
    uint32_t deadline_ms;
    uint8_t window;
    uint8_t index;
    uint8_t previous_intensity;
    bool configured;
} ForceFilter;

void force_filter_configure(ForceFilter *filter, uint8_t intensity);
int32_t force_filter_update(ForceFilter *filter, int32_t sample, uint32_t now_ms);

#endif
