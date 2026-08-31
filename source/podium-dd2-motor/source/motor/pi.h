#ifndef OPENTEC_MOTOR_PI_H
#define OPENTEC_MOTOR_PI_H

#include <gflib.h>

frac16_t motor_pi_step(frac16_t error, const bool_t *stop_integrator,
                       GFLIB_CTRL_PI_P_AW_T_A32 *controller);

#endif
