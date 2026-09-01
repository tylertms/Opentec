#ifndef OPENTEC_MOTOR_PI_H
#define OPENTEC_MOTOR_PI_H

#include "rtcesl.h"

/**
 * @brief Advances the motor parallel anti-windup PI controller.
 *
 * The controller updates its integral history, applies configured integral and output limits, and
 * publishes whether the output reached a limit.
 *
 * @param[in] error Current signed controller error.
 * @param[in] stop_integrator True to exclude the current error from the integration input.
 * @param[in,out] controller PI gains, limits, history, and limiter flag to update.
 * @return Signed controller output limited to the configured range.
 */
frac16_t motor_pi_step(frac16_t error, const bool_t *stop_integrator,
                       GFLIB_CTRL_PI_P_AW_T_A32 *controller);

#endif
