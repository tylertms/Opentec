#ifndef OPENTEC_MOTOR_VELOCITY_CONTROL_H
#define OPENTEC_MOTOR_VELOCITY_CONTROL_H

#include <gflib.h>
#include <stdint.h>

typedef struct {
    GFLIB_CTRL_PI_P_AW_T_A32 controller;
    GFLIB_RAMP_T_F32 target_ramp;
    int16_t target_velocity;
    int16_t ramped_velocity;
    int16_t velocity_error;
    int16_t current_reference;
    bool_t stop_integrator;
} MotorVelocityControlState;

void motor_velocity_control_initialize(MotorVelocityControlState *state, int16_t current_limit);
void motor_velocity_control_reset(MotorVelocityControlState *state);
void motor_velocity_control_target_set(MotorVelocityControlState *state, int16_t target_velocity);
int16_t motor_velocity_control_step(MotorVelocityControlState *state, int16_t measured_velocity,
                                    bool_t current_controller_limited);

#endif
