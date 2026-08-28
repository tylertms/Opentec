#include "wheel/motion.h"

#include <stdint.h>

static void accumulate(uint8_t *counter, int8_t delta) {
    if (delta < 0) {
        (*counter)--;
    } else if (delta > 0) {
        (*counter)++;
    }
}

static int8_t take(uint8_t *counter) {
    if (*counter == 0) {
        return 0;
    }
    if ((*counter & 0x80u) != 0) {
        (*counter)++;
        return -1;
    }
    (*counter)--;
    return 1;
}

/**
 * @brief Clears all attached-wheel motion counters.
 *
 * Resets the primary motion counter and both independently consumed axis counters to zero.
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
 * Updates either of the two wrapping axis counters from the sign of the input. Unsupported axis
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
