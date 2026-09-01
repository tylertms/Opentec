#ifndef OPENTEC_MOTOR_VELOCITY_CONTROL_H
#define OPENTEC_MOTOR_VELOCITY_CONTROL_H

#include "rtcesl.h"
#include <stdint.h>

/** @brief Persistent velocity target, ramp, PI controller, and output state. */
typedef struct {
    GFLIB_CTRL_PI_P_AW_T_A32 controller; /**< Anti-windup PI controller state. */
    GFLIB_RAMP_T_F32 target_ramp; /**< Ramp state for the requested velocity. */
    int16_t target_velocity; /**< Requested signed velocity. */
    int16_t ramped_velocity; /**< Current ramped velocity target. */
    int16_t velocity_error; /**< Difference between ramped and measured velocity. */
    int16_t current_reference; /**< Latest signed Q-axis current reference. */
    bool_t stop_integrator; /**< True when velocity or current control is limiting integration. */
} MotorVelocityControlState;

/**
 * @brief Initializes the calibration velocity controller.
 *
 * The controller limits are set from current_limit and all runtime history is cleared.
 *
 * @param[out] state Velocity-control state to initialize.
 * @param[in] current_limit Positive current limit; its negative is the lower limit.
 */
void motor_velocity_control_initialize(MotorVelocityControlState *state, int16_t current_limit);

/**
 * @brief Clears velocity target, ramp, output, and controller history.
 *
 * The configured controller gains and limits remain unchanged.
 *
 * @param[in,out] state Velocity-control state to reset.
 */
void motor_velocity_control_reset(MotorVelocityControlState *state);

/**
 * @brief Clears velocity PI integral, error, and limiter history.
 *
 * The configured controller gains and limits remain unchanged.
 *
 * @param[in,out] state Velocity-control state containing the PI controller.
 */
void motor_velocity_control_controller_reset(MotorVelocityControlState *state);

/**
 * @brief Sets the requested calibration velocity.
 *
 * The target is consumed by the ramp on the next velocity-control step.
 *
 * @param[in,out] state Velocity-control state to update.
 * @param[in] target_velocity Signed requested velocity.
 */
void motor_velocity_control_target_set(MotorVelocityControlState *state, int16_t target_velocity);

/**
 * @brief Advances the calibration velocity controller once.
 *
 * The ramped target and measured velocity produce a saturated PI current reference, with
 * integration stopped when either current or velocity control is limiting.
 *
 * @param[in,out] state Velocity-control state to update.
 * @param[in] measured_velocity Current filtered velocity measurement.
 * @param[in] current_controller_limited True when the current controller is saturated.
 * @return Signed Q-axis current reference.
 */
int16_t motor_velocity_control_step(MotorVelocityControlState *state, int16_t measured_velocity,
                                    bool_t current_controller_limited);

#endif
