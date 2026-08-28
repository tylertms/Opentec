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

/**
 * Applies the active brake-force gain to a digital pedal sample.
 *
 * @param value Raw brake-axis sample.
 * @param force_percent Configured brake force from zero to 100 percent.
 * @return Scaled brake sample saturated to the 16-bit axis range.
 */
uint16_t pedal_input_scale_brake(uint16_t value, uint8_t force_percent) {
    uint8_t force = force_percent > 100 ? 100 : force_percent;
    uint32_t gain_percent = 100u + (uint32_t)(100 - force) * 4u;
    uint32_t scaled = (uint32_t)value * gain_percent / 100u;
    return scaled > UINT16_MAX ? UINT16_MAX : (uint16_t)scaled;
}

uint16_t pedal_input_hid_axis(uint16_t value) { return (uint16_t)~value; }

uint8_t pedal_input_hid_auxiliary(uint8_t value) { return (uint8_t)~value; }
