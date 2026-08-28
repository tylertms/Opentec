#include "force_feedback/script_motion.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_range.h"

enum {
    MOTION_SELECTOR = 0,
    MOTION_POSITION = 4,
    MOTION_VELOCITY = 5,
    MOTION_ACCELERATION = 6,
    MOTION_ANGLE = 7,
    FORCE_FEEDBACK_TICKS_PER_SECOND = 10000,
};

typedef union {
    float number;
    uint32_t bits;
} MotionValue;

static float read_value(uint32_t bits) { return (MotionValue){.bits = bits}.number; }

static uint32_t value_bits(float value) { return (MotionValue){.number = value}.bits; }

static float clamp_position(float position) {
    if (position > 1.0f) {
        return 1.0f;
    }
    if (position < -1.0f) {
        return -1.0f;
    }
    return position;
}

/**
 * @brief Update script motion values from wheel position or live integration input.
 *
 * Uses motion value 0 as an input selector. A selector matching one of the three live-input slot
 * statuses integrates that slot's floating-point duration into the normalized position and clamps
 * it to -1 through 1. Selector zero samples wheel position when no slot matches; other unmatched
 * selectors retain the previous position. The function then derives angle, velocity, and
 * acceleration from the configured rotation range and 10 kHz motion clock and mirrors position,
 * angle, velocity, and acceleration into axes 0 through 3.
 *
 * @param[in,out] runtime Script motion values, rotation range, and axes.
 * @param[in] inputs Three live integration inputs and their selector statuses.
 * @param[in,out] state Previous position, velocity, and motion-clock snapshot.
 * @param[in] motion_ticks Current 10 kHz motion-clock count.
 * @param[in] wheel_position Signed raw wheel-position sample.
 * @param[in] half_travel Positive raw wheel travel from center to either endpoint.
 * @param[in] integrate_inputs true to apply a matching live integration slot; false to use only
 * wheel sampling or the retained position.
 * @pre runtime, inputs, and state point to valid objects.
 * @pre half_travel is nonzero when selector zero does not match a live-input slot.
 */
void force_feedback_script_motion_update(ForceFeedbackScriptRuntime *runtime,
                                         const ForceFeedbackScriptInputs *inputs,
                                         ForceFeedbackScriptMotionState *state,
                                         uint32_t motion_ticks, int32_t wheel_position,
                                         uint32_t half_travel, bool integrate_inputs) {
    uint32_t selector = runtime->motion[MOTION_SELECTOR];
    float position = read_value(runtime->motion[MOTION_POSITION]);
    bool integrated = false;
    if (integrate_inputs) {
        for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT; index++) {
            if (selector == inputs->slots[index].status) {
                position = clamp_position(position + read_value(inputs->slots[index].duration));
                integrated = true;
                break;
            }
        }
    }
    if (!integrated && selector == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION) {
        position = (float)wheel_position / (float)half_travel;
    }

    float tick_delta = (float)(motion_ticks - state->tick_snapshot);
    float velocity =
        (position - state->previous_position) * (float)FORCE_FEEDBACK_TICKS_PER_SECOND / tick_delta;
    float acceleration =
        (velocity - state->previous_velocity) * (float)FORCE_FEEDBACK_TICKS_PER_SECOND / tick_delta;
    float angle = force_feedback_script_rotation_scale(position, runtime->rotation_range_code,
                                                       runtime->extended_rotation_range);

    runtime->motion[MOTION_POSITION] = value_bits(position);
    runtime->motion[MOTION_VELOCITY] = value_bits(velocity);
    runtime->motion[MOTION_ACCELERATION] = value_bits(acceleration);
    runtime->motion[MOTION_ANGLE] = value_bits(angle);
    runtime->axes[0] = value_bits(position);
    runtime->axes[1] = value_bits(angle);
    runtime->axes[2] = value_bits(velocity);
    runtime->axes[3] = value_bits(acceleration);
    state->tick_snapshot = motion_ticks;
    state->previous_position = position;
    state->previous_velocity = velocity;
}
