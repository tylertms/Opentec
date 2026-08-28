#include "pedal/input.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_V3_CONNECTION_REPORT = 4,
    PEDAL_V3_CALIBRATION_REPORT = 5,
    PEDAL_V3_BRAKE_FORCE_REPORT = 7,
    PEDAL_V3_SHARED_AXES_REPORT = 8,
    PEDAL_V3_PRIMARY_CALIBRATION = 0x6205,
    PEDAL_V3_LEGACY_CALIBRATION = 0x183b,
    PEDAL_V3_ACTIVE_CALIBRATION = 0x6204,
    PEDAL_V3_SECONDARY_CALIBRATION = 0x6206,
    PEDAL_V3_NORMAL_BRAKE_STEP = 10,
    PEDAL_V3_CALIBRATION_BRAKE_STEP = 5,
};

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

void pedal_v3_state_init(PedalV3State *state) { *state = (PedalV3State){0}; }

/**
 * @brief Applies a recognized V3 input report to the pedal state.
 * @param frame Decoded V3 frame to process.
 * @param auxiliary_locked True when another input source owns the auxiliary axis.
 * @param state V3 connection, calibration, force, and shared-axis state to update.
 * @param input Published pedal axes to update for axis reports.
 * @return True for a recognized V3 report type.
 */
bool pedal_v3_apply_report(const PedalFrame *frame, bool auxiliary_locked, PedalV3State *state,
                           PedalInput *input) {
    switch (frame->type) {
    case PEDAL_FRAME_AXIS_SAMPLE:
        input->axes[0] = read_u16(frame->payload);
        state->raw_brake = read_u16(frame->payload + 2);
        input->axes[1] = state->raw_brake;
        input->axes[2] = read_u16(frame->payload + 4);
        if (!auxiliary_locked) {
            input->auxiliary = frame->payload[7];
        }
        return true;
    case PEDAL_V3_CONNECTION_REPORT:
        state->connection_flags = frame->payload[0];
        return true;
    case PEDAL_V3_CALIBRATION_REPORT: {
        uint16_t mode = read_u16(frame->payload);
        if (mode == PEDAL_V3_PRIMARY_CALIBRATION) {
            state->primary_calibration = true;
            state->legacy_calibration = false;
        } else if (mode == PEDAL_V3_LEGACY_CALIBRATION) {
            state->legacy_calibration = true;
        } else if (mode == PEDAL_V3_ACTIVE_CALIBRATION) {
            state->primary_calibration = true;
        } else if (mode == PEDAL_V3_SECONDARY_CALIBRATION) {
            state->legacy_calibration = false;
            state->secondary_calibration = true;
        }
        return true;
    }
    case PEDAL_V3_BRAKE_FORCE_REPORT: {
        bool fine_scale = (state->primary_calibration && !state->legacy_calibration) ||
                          state->secondary_calibration;
        uint8_t step = fine_scale ? PEDAL_V3_CALIBRATION_BRAKE_STEP : PEDAL_V3_NORMAL_BRAKE_STEP;
        state->alternate_brake_force = (uint8_t)((uint8_t)(frame->payload[0] - 1u) * step);
        return true;
    }
    case PEDAL_V3_SHARED_AXES_REPORT:
        for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
            state->shared_axes[axis] = frame->payload[axis];
        }
        return true;
    default:
        return false;
    }
}

/**
 * Applies the active brake-force gain to a digital pedal sample.
 *
 * @param[in] value Raw brake-axis sample.
 * @param[in] force_percent Signed one-byte brake-force setting.
 * @return Scaled brake sample saturated to the 16-bit axis range.
 */
uint16_t pedal_input_scale_brake(uint16_t value, uint8_t force_percent) {
    int16_t force = (int8_t)force_percent;
    float gain = 1.0f + (float)(100 - force) * 0.04f;
    uint32_t scaled = (uint32_t)((float)value * gain);
    return scaled > UINT16_MAX ? UINT16_MAX : (uint16_t)scaled;
}

uint16_t pedal_input_hid_axis(uint16_t value) { return (uint16_t)~value; }

uint8_t pedal_input_hid_auxiliary(uint8_t value) { return (uint8_t)~value; }
