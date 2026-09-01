#include "pedal/input.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief V3 report identifiers and calibration constants.
 */
enum {
    PEDAL_V3_CONNECTION_REPORT = 4,          /**< V3 connection-flags report type. */
    PEDAL_V3_CALIBRATION_REPORT = 5,         /**< V3 calibration-mode report type. */
    PEDAL_V3_SHARED_AXES_REPORT = 8,         /**< V3 shared-axis report type. */
    PEDAL_V3_PRIMARY_CALIBRATION = 0x6205,   /**< Primary calibration mode value. */
    PEDAL_V3_LEGACY_CALIBRATION = 0x183b,    /**< Legacy calibration mode value. */
    PEDAL_V3_ACTIVE_CALIBRATION = 0x6204,    /**< Active primary calibration mode value. */
    PEDAL_V3_SECONDARY_CALIBRATION = 0x6206, /**< Secondary calibration mode value. */
    PEDAL_V3_NORMAL_BRAKE_STEP = 10,         /**< Brake-force step outside fine calibration. */
    PEDAL_V3_CALIBRATION_BRAKE_STEP = 5,     /**< Brake-force step during fine calibration. */
};

/**
 * @brief Reads a little-endian pedal-axis value.
 *
 * Combines two consecutive bytes with the least significant byte first.
 *
 * @param[in] data Two-byte axis field.
 * @return The decoded unsigned axis value.
 */
static uint16_t read_u16(const uint8_t *data) { return (uint16_t)data[0] | (uint16_t)data[1] << 8; }

static uint16_t normalize_axis(uint16_t value) {
    return value == UINT16_C(0xff00) ? UINT16_MAX : value;
}

/**
 * @brief Releases every published pedal input.
 *
 * Clears all three pedal axes and the auxiliary input to their internal released values.
 *
 * @param[out] input Published pedal input state.
 */
void pedal_input_release(PedalInput *input) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        input->axes[axis] = 0;
    }
    input->auxiliary = 0;
}

/**
 * @brief Decodes a pedal axis-sample report.
 *
 * Reads three little-endian 16-bit axes and the final auxiliary byte. Other report types leave the
 * destination unchanged.
 *
 * @param[in] frame Decoded pedal frame.
 * @param[in,out] input Published pedal input state; unchanged for non-axis reports.
 * @return true for an axis-sample report; otherwise false.
 */
bool pedal_input_decode(const PedalFrame *frame, PedalInput *input) {
    if (frame->type != PEDAL_FRAME_AXIS_SAMPLE) {
        return false;
    }

    input->axes[0] = normalize_axis(read_u16(frame->payload));
    input->axes[1] = normalize_axis(read_u16(frame->payload + 2));
    input->axes[2] = normalize_axis(read_u16(frame->payload + 4));
    input->auxiliary = frame->payload[7];
    return true;
}

/**
 * @brief Initializes V3 pedal protocol state.
 *
 * Clears connection, calibration, brake-force, raw-brake, and shared-axis state.
 *
 * @param[out] state V3 pedal state to initialize.
 */
void pedal_v3_state_init(PedalV3State *state) { *state = (PedalV3State){0}; }

/**
 * @brief Applies a recognized V3 input report to the pedal state.
 *
 * Decodes axis, connection, calibration, brake-force, and shared-axis reports while preserving an
 * auxiliary input owned by another source.
 *
 * @param[in] frame Decoded V3 frame to process.
 * @param[in] auxiliary_locked True when another input source owns the auxiliary axis.
 * @param[in,out] state V3 connection, calibration, force, and shared-axis state to update.
 * @param[in,out] input Published pedal axes to update for axis reports.
 * @return True for a recognized V3 report type.
 */
bool pedal_v3_apply_report(const PedalFrame *frame, bool auxiliary_locked, PedalV3State *state,
                           PedalInput *input) {
    switch (frame->type) {
    case PEDAL_FRAME_AXIS_SAMPLE:
        input->axes[0] = normalize_axis(read_u16(frame->payload));
        state->raw_brake = normalize_axis(read_u16(frame->payload + 2));
        input->axes[1] = state->raw_brake;
        input->axes[2] = normalize_axis(read_u16(frame->payload + 4));
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
 * @brief Applies the configured brake-force gain to a digital pedal sample.
 *
 * Increases the sample gain as the configured force decreases and saturates the result to the
 * published 16-bit axis range.
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

/**
 * @brief Converts an internal pedal axis to its HID representation.
 *
 * Complements all sixteen bits so an internal released value of zero is published as 65535.
 *
 * @param[in] value Internal pedal-axis value.
 * @return The complemented HID axis value.
 */
uint16_t pedal_input_hid_axis(uint16_t value) { return (uint16_t)~value; }

/**
 * @brief Converts the internal auxiliary input to its HID representation.
 *
 * Complements all eight bits so an internal released value of zero is published as 255.
 *
 * @param[in] value Internal auxiliary-input value.
 * @return The complemented HID auxiliary value.
 */
uint8_t pedal_input_hid_auxiliary(uint8_t value) { return (uint8_t)~value; }
