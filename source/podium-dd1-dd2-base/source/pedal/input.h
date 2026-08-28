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

typedef struct {
    uint16_t raw_brake;
    uint8_t connection_flags;
    uint8_t alternate_brake_force;
    uint8_t shared_axes[PEDAL_INPUT_AXIS_COUNT];
    bool primary_calibration;
    bool legacy_calibration;
    bool secondary_calibration;
} PedalV3State;

void pedal_input_release(PedalInput *input);
bool pedal_input_decode(const PedalFrame *frame, PedalInput *input);
void pedal_v3_state_init(PedalV3State *state);
bool pedal_v3_apply_report(const PedalFrame *frame, bool auxiliary_locked, PedalV3State *state,
                           PedalInput *input);
uint16_t pedal_input_scale_brake(uint16_t value, uint8_t force_percent);
uint16_t pedal_input_hid_axis(uint16_t value);
uint8_t pedal_input_hid_auxiliary(uint8_t value);

#endif
