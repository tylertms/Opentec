#include "motor/velocity_control.h"

#include <limits.h>
#include "motor/pi.h"

/** @brief Fixed-point gains used by calibration velocity control. */
enum {
    MOTOR_VELOCITY_PROPORTIONAL_GAIN = 0x14000, /**< Velocity-loop proportional gain. */
    MOTOR_VELOCITY_INTEGRAL_GAIN = 0x617, /**< Velocity-loop integral gain. */
};

/**
 * @brief Initializes the calibration velocity controller.
 *
 * The controller uses the product current limit with fixed-point ramp and anti-windup PI settings.
 *
 * @param[out] state Velocity target, ramp, PI controller, and published control values.
 * @param[in] current_limit Product-specific positive current limit; its negative is the lower
 * limit.
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
 * @brief Clears all calibration velocity-controller runtime history.
 *
 * Target, ramp, error, output, integration-stop, integrator, previous-error, and limiter state are
 * reset before a fresh calibration run.
 *
 * @param[in,out] state Velocity controller to reset.
 */
void motor_velocity_control_reset(MotorVelocityControlState *state) {
    state->target_velocity = 0;
    state->target_ramp.f32State = 0;
    state->ramped_velocity = 0;
    state->velocity_error = 0;
    state->current_reference = 0;
    state->stop_integrator = 0U;
    motor_velocity_control_controller_reset(state);
    state->controller.bLimFlag = 0U;
}

/**
 * @brief Clears only the calibration velocity PI controller history.
 *
 * The integral accumulator and previous input error return to zero. The velocity target, ramp,
 * output, controller gains and limits, and published limiter flag are preserved.
 *
 * @param[in,out] state Velocity controller containing the PI state.
 */
void motor_velocity_control_controller_reset(MotorVelocityControlState *state) {
    GFLIB_CtrlPIpAWInit_F16(0, &state->controller);
}

/**
 * @brief Selects the requested calibration velocity.
 *
 * The service-rate controller consumes this target independently from the ADC-rate calibration
 * state machine.
 *
 * @param[in,out] state Active velocity controller.
 * @param[in] target_velocity Signed filtered encoder delta requested by calibration.
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
 * @param[in,out] state Persistent velocity ramp and PI controller.
 * @param[in] measured_velocity Shift-four filtered encoder delta from the periodic motion
 * estimator.
 * @param[in] current_controller_limited Current-loop PI saturation flag.
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
        motor_pi_step(state->velocity_error, &state->stop_integrator, &state->controller);
    return state->current_reference;
}
