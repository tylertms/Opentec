#include "motor/rotation_guard.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Duration of the maximum-rotation runtime hold in milliseconds.
 */
enum {
    MOTOR_ROTATION_GUARD_HOLD_MS = 4500 /**< Hold interval before the warning is emitted. */
};

/**
 * @brief Initializes maximum-rotation monitoring.
 *
 * Clears the observed motor runtime, hold deadline, and persistent warning latch.
 *
 * @param[out] guard Maximum-rotation monitor to initialize.
 */
void motor_rotation_guard_init(MotorRotationGuard *guard) { *guard = (MotorRotationGuard){0}; }

/**
 * @brief Detects a stalled motor runtime at the minimum steering limit.
 *
 * Starts a 4.5-second observation when the published steering axis reaches zero or its invalid
 * sentinel and the motor runtime is nonzero. A changed motor runtime restarts the observation. An
 * unchanged runtime after the strict deadline produces one persistent maximum-rotation warning.
 *
 * @param[in,out] guard Maximum-rotation observation state.
 * @param[in] steering_axis Published unsigned steering axis or invalid sentinel.
 * @param[in] motor_runtime_seconds Latest motor-controller runtime counter.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True once when the persistent warning must be shown.
 */
bool motor_rotation_guard_update(MotorRotationGuard *guard, int32_t steering_axis,
                                 uint32_t motor_runtime_seconds, uint32_t now_ms) {
    if (guard->triggered || (steering_axis != -1 && steering_axis != 0)) {
        return false;
    }
    if (!guard->monitoring) {
        if (motor_runtime_seconds == 0) {
            return false;
        }
        guard->observed_runtime_seconds = motor_runtime_seconds;
        guard->deadline_ms = now_ms + MOTOR_ROTATION_GUARD_HOLD_MS;
        guard->monitoring = true;
        return false;
    }
    if ((int32_t)(now_ms - guard->deadline_ms) <= 0) {
        return false;
    }
    if (motor_runtime_seconds != guard->observed_runtime_seconds) {
        guard->monitoring = false;
        return false;
    }

    guard->triggered = true;
    return true;
}
