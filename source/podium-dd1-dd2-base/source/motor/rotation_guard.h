#ifndef OPENTEC_BASE_MOTOR_ROTATION_GUARD_H
#define OPENTEC_BASE_MOTOR_ROTATION_GUARD_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Motor-runtime hold state for the persistent maximum-rotation warning.
 *
 * Tracks the runtime sample and timed hold used to detect a stalled motor at the steering limit.
 */
typedef struct {
    uint32_t observed_runtime_seconds; /**< Runtime sample captured when monitoring began. */
    uint32_t deadline_ms;              /**< Monotonic deadline for the current hold interval. */
    bool monitoring; /**< True while a runtime sample is being held for comparison. */
    bool triggered;  /**< True after the persistent maximum-rotation warning was emitted. */
} MotorRotationGuard;

/**
 * @brief Resets maximum-rotation monitoring.
 *
 * Clears the held runtime, deadline, monitoring state, and one-shot warning latch.
 *
 * @param[out] guard Maximum-rotation monitoring state to initialize.
 */
void motor_rotation_guard_init(MotorRotationGuard *guard);

/**
 * @brief Checks for a stalled motor at the steering limit.
 *
 * Starts a timed observation when the steering axis is at zero or its invalid sentinel and emits a
 * one-shot warning after the motor runtime remains unchanged past the hold deadline.
 *
 * @param[in,out] guard Maximum-rotation monitoring state.
 * @param[in] steering_axis Published steering axis value or invalid sentinel.
 * @param[in] motor_runtime_seconds Latest motor-controller runtime counter.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True once when the persistent maximum-rotation warning must be shown.
 */
bool motor_rotation_guard_update(MotorRotationGuard *guard, int32_t steering_axis,
                                 uint32_t motor_runtime_seconds, uint32_t now_ms);

#endif
