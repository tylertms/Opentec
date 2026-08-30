#ifndef OPENTEC_BASE_PEDAL_ADJUSTMENT_PROBE_H
#define OPENTEC_BASE_PEDAL_ADJUSTMENT_PROBE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_ADJUSTMENT_PROBE_REQUEST_SIZE = 25,
};

typedef enum {
    PEDAL_ADJUSTMENT_DISPLAY_IDLE = 0,
    PEDAL_ADJUSTMENT_DISPLAY_HOLD = 0x80,
    PEDAL_ADJUSTMENT_DISPLAY_NONE = 0x90,
    PEDAL_ADJUSTMENT_DISPLAY_BOTH = 0x91,
    PEDAL_ADJUSTMENT_DISPLAY_THROTTLE = 0x93,
    PEDAL_ADJUSTMENT_DISPLAY_CLUTCH = 0x95,
} PedalAdjustmentDisplay;

const uint8_t *pedal_adjustment_probe_request(void);
bool pedal_adjustment_probe_classify(const uint8_t *response, uint8_t length,
                                     PedalAdjustmentDisplay *display);

#endif
