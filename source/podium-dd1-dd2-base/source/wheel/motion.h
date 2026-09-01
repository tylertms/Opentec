#ifndef OPENTEC_BASE_WHEEL_MOTION_H
#define OPENTEC_BASE_WHEEL_MOTION_H

#include <stdint.h>

/** @brief Number of independently consumable auxiliary motion axes. */
enum {
    WHEEL_MOTION_AXIS_COUNT = 4 /**< Number of independently consumable auxiliary motion axes. */
};

/** @brief Wrapping counters for primary and auxiliary attached-wheel motion. */
typedef struct {
    uint8_t primary;                       /**< Queued primary motion count. */
    uint8_t axes[WHEEL_MOTION_AXIS_COUNT]; /**< Queued count for each auxiliary axis. */
} WheelMotion;

/**
 * @brief Clears all queued attached-wheel motion.
 *
 * Resets the primary counter and every auxiliary-axis counter to zero.
 *
 * @param[out] motion Motion state to initialize.
 */
void wheel_motion_init(WheelMotion *motion);

/**
 * @brief Adds one primary motion step.
 *
 * Uses only the sign of @p delta: negative decrements the wrapping counter and positive
 * increments it.
 *
 * @param[in,out] motion Motion state to update.
 * @param[in] delta Signed input selecting the direction; zero has no effect.
 */
void wheel_motion_accumulate_primary(WheelMotion *motion, int8_t delta);

/**
 * @brief Adds one auxiliary-axis motion step.
 *
 * Updates the selected wrapping counter from the sign of @p delta. Unsupported axis values are
 * ignored.
 *
 * @param[in,out] motion Motion state to update.
 * @param[in] axis Zero-based auxiliary axis index.
 * @param[in] delta Signed input selecting the direction; zero has no effect.
 */
void wheel_motion_accumulate_axis(WheelMotion *motion, uint8_t axis, int8_t delta);

/**
 * @brief Reads the queued primary motion direction.
 *
 * Does not consume the primary counter.
 *
 * @param[in] motion Motion state to inspect.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_motion_primary_direction(const WheelMotion *motion);

/**
 * @brief Reads one queued auxiliary-axis direction.
 *
 * Does not consume the selected counter. Unsupported axis values return zero.
 *
 * @param[in] motion Motion state to inspect.
 * @param[in] axis Zero-based auxiliary axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_motion_axis_direction(const WheelMotion *motion, uint8_t axis);

/**
 * @brief Consumes one queued primary motion step.
 *
 * Moves the wrapping primary counter one step toward zero.
 *
 * @param[in,out] motion Motion state to consume.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_motion_take_primary(WheelMotion *motion);

/**
 * @brief Consumes one queued auxiliary-axis motion step.
 *
 * Moves the selected wrapping counter one step toward zero. Unsupported axis values are ignored.
 *
 * @param[in,out] motion Motion state to consume.
 * @param[in] axis Zero-based auxiliary axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_motion_take_axis(WheelMotion *motion, uint8_t axis);

#endif
