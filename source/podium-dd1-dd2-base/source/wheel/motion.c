#include "wheel/motion.h"

#include <stdint.h>

/**
 * @brief Accumulates one signed motion direction.
 *
 * Moves the wrapping counter by one according to the input sign and ignores zero.
 *
 * @param[in,out] counter Wrapping motion counter to update.
 * @param[in] delta Signed input used only to select the step direction.
 */
static void accumulate(uint8_t *counter, int8_t delta) {
    if (delta < 0) {
        (*counter)--;
    } else if (delta > 0) {
        (*counter)++;
    }
}

/**
 * @brief Returns the direction represented by a wrapping counter.
 *
 * Interprets zero as idle, the lower half as positive, and the upper half as negative.
 *
 * @param[in] counter Wrapping motion counter.
 * @return Negative one, zero, or positive one.
 */
static int8_t direction(uint8_t counter) {
    if (counter == 0) {
        return 0;
    }
    return (counter & 0x80u) != 0 ? -1 : 1;
}

/**
 * @brief Takes one signed motion direction.
 *
 * Moves a nonzero wrapping counter one position toward zero and returns its direction.
 *
 * @param[in,out] counter Wrapping motion counter to consume.
 * @return Negative one, zero, or positive one.
 */
static int8_t take(uint8_t *counter) {
    int8_t step = direction(*counter);
    if (step < 0) {
        (*counter)++;
    } else if (step > 0) {
        (*counter)--;
    }
    return step;
}

/**
 * @brief Clears all attached-wheel motion counters.
 *
 * Resets the primary motion counter and four independently consumed axis counters to zero.
 *
 * @param[out] motion Motion accumulator to initialize.
 */
void wheel_motion_init(WheelMotion *motion) {
    motion->primary = 0;
    for (uint8_t axis = 0; axis < WHEEL_MOTION_AXIS_COUNT; axis++) {
        motion->axes[axis] = 0;
    }
}

/**
 * @brief Accumulates one primary wheel motion step.
 *
 * Decrements the wrapping counter for any negative input and increments it for any positive input.
 * A zero input leaves the counter unchanged.
 *
 * @param[in,out] motion Motion state whose primary counter is updated.
 * @param[in] delta Signed input magnitude used only to select the step direction.
 */
void wheel_motion_accumulate_primary(WheelMotion *motion, int8_t delta) {
    accumulate(&motion->primary, delta);
}

/**
 * @brief Accumulates one auxiliary wheel motion step.
 *
 * Updates one of the four wrapping axis counters from the sign of the input. Unsupported axis
 * indices leave the state unchanged.
 *
 * @param[in,out] motion Motion state whose selected axis counter is updated.
 * @param[in] axis Zero-based axis index.
 * @param[in] delta Signed input magnitude used only to select the step direction.
 */
void wheel_motion_accumulate_axis(WheelMotion *motion, uint8_t axis, int8_t delta) {
    if (axis < WHEEL_MOTION_AXIS_COUNT) {
        accumulate(&motion->axes[axis], delta);
    }
}

/**
 * @brief Returns the queued primary wheel motion direction.
 *
 * Inspects the primary wrapping counter without consuming it.
 *
 * @param[in] motion Motion state whose primary direction is requested.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_motion_primary_direction(const WheelMotion *motion) {
    return direction(motion->primary);
}

/**
 * @brief Takes one queued primary wheel motion step.
 *
 * Moves the wrapping primary counter one position toward zero and returns its signed direction.
 *
 * @param[in,out] motion Motion state whose primary counter is consumed.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_motion_take_primary(WheelMotion *motion) { return take(&motion->primary); }

/**
 * @brief Takes one queued auxiliary wheel motion step.
 *
 * Moves the selected wrapping axis counter one position toward zero. Unsupported axis indices do
 * not modify the state.
 *
 * @param[in,out] motion Motion state whose selected axis counter is consumed.
 * @param[in] axis Zero-based axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_motion_take_axis(WheelMotion *motion, uint8_t axis) {
    return axis < WHEEL_MOTION_AXIS_COUNT ? take(&motion->axes[axis]) : 0;
}
