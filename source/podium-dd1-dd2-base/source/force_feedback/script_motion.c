#include "force_feedback/script_motion.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_range.h"

/**
 * @brief Runtime indexes and scale used by motion updates.
 *
 * The indexes identify script motion values, and the clock constant converts 10 kHz ticks to
 * seconds.
 */
enum {
    MOTION_SELECTOR = 0,                     /**< Motion selector value index. */
    MOTION_POSITION = 4,                     /**< Normalized position value index. */
    MOTION_VELOCITY = 5,                     /**< Velocity value index. */
    MOTION_ACCELERATION = 6,                 /**< Acceleration value index. */
    MOTION_ANGLE = 7,                        /**< Rotation angle value index. */
    FORCE_FEEDBACK_TICKS_PER_SECOND = 10000, /**< Motion-clock frequency in ticks per second. */
};

/**
 * @brief Provides numeric and raw-bit views of a motion value.
 *
 * The motion update preserves script value bits while converting them to and from floating-point
 * values for calculations.
 */
typedef union {
    float number;  /**< Single-precision numeric view. */
    uint32_t bits; /**< Raw 32-bit representation. */
} MotionValue;

/**
 * @brief Interprets a raw motion value as floating point.
 *
 * Preserves all bits while changing only the C representation used by the motion update.
 *
 * @param[in] bits Raw 32-bit representation.
 * @return The represented single-precision value.
 */
static float read_value(uint32_t bits) { return (MotionValue){.bits = bits}.number; }

/**
 * @brief Returns the raw representation of a motion value.
 *
 * Preserves all bits of the single-precision value.
 *
 * @param[in] value Floating-point motion value.
 * @return The raw 32-bit representation.
 */
static uint32_t value_bits(float value) { return (MotionValue){.number = value}.bits; }

/**
 * @brief Limits a normalized position.
 *
 * Values above one become one and values below negative one become negative one.
 *
 * @param[in] position Position to limit.
 * @return The position limited to the inclusive range from -1 to 1.
 */
static float clamp_position(float position) {
    if (position > 1.0f) {
        return 1.0f;
    }
    if (position < -1.0f) {
        return -1.0f;
    }
    return position;
}

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
