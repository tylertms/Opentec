#ifndef OPENTEC_BASE_MOTOR_ROTATION_GUARD_H
#define OPENTEC_BASE_MOTOR_ROTATION_GUARD_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Motor-runtime hold state for the persistent maximum-rotation warning. */
typedef struct {
    uint32_t observed_runtime_seconds;
    uint32_t deadline_ms;
    bool monitoring;
    bool triggered;
} MotorRotationGuard;

void motor_rotation_guard_init(MotorRotationGuard *guard);
bool motor_rotation_guard_update(MotorRotationGuard *guard, int32_t steering_axis,
                                 uint32_t motor_runtime_seconds, uint32_t now_ms);

#endif
