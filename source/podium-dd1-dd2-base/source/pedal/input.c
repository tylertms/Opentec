#include "pedal/input.h"

#include <stdbool.h>
#include <stdint.h>

static uint16_t read_u16(const uint8_t *data) { return (uint16_t)data[0] | (uint16_t)data[1] << 8; }

void pedal_input_release(PedalInput *input) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        input->axes[axis] = 0;
    }
    input->auxiliary = 0;
}

bool pedal_input_decode(const PedalFrame *frame, PedalInput *input) {
    if (frame->type != PEDAL_FRAME_AXIS_SAMPLE) {
        return false;
    }

    input->axes[0] = read_u16(frame->payload);
    input->axes[1] = read_u16(frame->payload + 2);
    input->axes[2] = read_u16(frame->payload + 4);
    input->auxiliary = frame->payload[7];
    return true;
}

uint16_t pedal_input_hid_axis(uint16_t value) { return (uint16_t)~value; }
