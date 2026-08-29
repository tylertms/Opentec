#include "common/motor/velocity_control.h"

#include <limits.h>
#include <mlib.h>

enum {
    MOTOR_VELOCITY_PROPORTIONAL_GAIN = 0x14000,
    MOTOR_VELOCITY_INTEGRAL_GAIN = 0x617,
};

/**
 * @brief Initializes the calibration velocity controller.
 *
 * The controller uses the product current limit and the NXP Q31 ramp and anti-windup PI settings
 * recovered identically from the DD1 and DD2 motor images.
 *
 * @param state Velocity target, ramp, PI controller, and published control values.
 * @param current_limit Product-specific positive current limit; its negative is the lower limit.
 */
void motor_velocity_control_initialize(MotorVelocityControlState *state, int16_t current_limit) {
    *state = (MotorVelocityControlState){
        .controller =
            {
                .a32PGain = MOTOR_VELOCITY_PROPORTIONAL_GAIN,
                .a32IGain = MOTOR_VELOCITY_INTEGRAL_GAIN,
                .f16UpperLim = current_limit,
                .f16LowerLim = (int16_t)-current_limit,
            },
        .target_ramp =
            {
                .f32RampUp = INT32_MAX,
                .f32RampDown = INT32_MAX,
            },
    };
    motor_velocity_control_reset(state);
}

/**
 * @brief Resets the calibration velocity PI state.
 *
 * The target and published control values return to zero while the recovered gains, limits, and
 * ramp rates remain configured.
 *
 * @param state Velocity controller to reset.
 */
void motor_velocity_control_reset(MotorVelocityControlState *state) {
    state->target_velocity = 0;
    state->ramped_velocity = 0;
    state->velocity_error = 0;
    state->current_reference = 0;
    state->stop_integrator = 0;
    GFLIB_CtrlPIpAWInit_F16(0, &state->controller);
}

/**
 * @brief Selects the requested calibration velocity.
 *
 * The service-rate controller consumes this target independently from the ADC-rate calibration
 * state machine.
 *
 * @param state Active velocity controller.
 * @param target_velocity Signed filtered encoder delta requested by calibration.
 */
void motor_velocity_control_target_set(MotorVelocityControlState *state, int16_t target_velocity) {
    state->target_velocity = target_velocity;
}

/**
 * @brief Advances the calibration velocity controller once.
 *
 * The requested velocity passes through the NXP Q31 ramp, its saturated error drives the NXP
 * parallel PI controller, and either current-loop or velocity-loop limiting stops integration.
 *
 * @param state Persistent velocity ramp and PI controller.
 * @param measured_velocity Shift-four filtered encoder delta from the periodic motion estimator.
 * @param current_controller_limited Current-loop PI saturation flag.
 * @return Signed Q-axis current reference for the field-oriented controller.
 */
int16_t motor_velocity_control_step(MotorVelocityControlState *state, int16_t measured_velocity,
                                    bool_t current_controller_limited) {
    state->stop_integrator =
        (bool_t)((current_controller_limited | state->controller.bLimFlag) & UINT8_MAX);
    frac32_t ramped = GFLIB_Ramp_F32(MLIB_Conv_F32s(state->target_velocity), &state->target_ramp);
    state->ramped_velocity = MLIB_Conv_F16l(ramped);
    state->velocity_error = MLIB_SubSat_F16(state->ramped_velocity, measured_velocity);
    state->current_reference =
        GFLIB_CtrlPIpAW_F16(state->velocity_error, &state->stop_integrator, &state->controller);
    return state->current_reference;
}
