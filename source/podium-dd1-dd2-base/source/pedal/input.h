#ifndef OPENTEC_BASE_PEDAL_INPUT_H
#define OPENTEC_BASE_PEDAL_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"

enum {
    PEDAL_INPUT_AXIS_COUNT = 3,
    PEDAL_FRAME_AXIS_SAMPLE = 1,
};

typedef struct {
    uint16_t axes[PEDAL_INPUT_AXIS_COUNT];
    uint8_t auxiliary;
} PedalInput;

void pedal_input_release(PedalInput *input);
bool pedal_input_decode(const PedalFrame *frame, PedalInput *input);
uint16_t pedal_input_hid_axis(uint16_t value);
uint8_t pedal_input_hid_auxiliary(uint8_t value);

#endif
