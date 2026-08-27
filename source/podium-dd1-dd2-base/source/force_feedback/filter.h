#ifndef OPENTEC_BASE_FORCE_FEEDBACK_FILTER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_FILTER_H

#include <stdint.h>

enum { FORCE_FILTER_MAXIMUM_WINDOW = 40 };

typedef struct {
    int32_t samples[FORCE_FILTER_MAXIMUM_WINDOW];
    int64_t total;
    uint8_t window;
    uint8_t index;
} ForceFilter;

void force_filter_configure(ForceFilter *filter, uint8_t intensity);
int32_t force_filter_update(ForceFilter *filter, int32_t sample);

#endif
