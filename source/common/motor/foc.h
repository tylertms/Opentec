#ifndef OPENTEC_MOTOR_FOC_H
#define OPENTEC_MOTOR_FOC_H

#include <gdflib.h>
#include <gflib.h>
#include <gmclib.h>

typedef struct {
    GFLIB_CTRL_PI_P_AW_T_A32 d_controller;
    GFLIB_CTRL_PI_P_AW_T_A32 q_controller;
    GDFLIB_FILTER_IIR1_T_F32 q_current_filter;
    GDFLIB_FILTER_IIR1_T_F32 d_current_filter;
    bool_t stop_d_integrator;
    bool_t stop_q_integrator;
} MotorFocState;

typedef struct {
    GMCLIB_3COOR_T_F16 phase_current;
    GMCLIB_2COOR_DQ_T_F16 current_reference;
    GMCLIB_2COOR_SINCOS_T_F16 rotor_sin_cos;
    frac16_t dc_bus_voltage;
} MotorFocInput;

typedef struct {
    GMCLIB_2COOR_DQ_T_F16 filtered_current;
    GMCLIB_2COOR_DQ_T_F16 voltage;
    GMCLIB_3COOR_T_F16 duty;
    uint16_t sector;
} MotorFocOutput;

void motor_foc_step(MotorFocState *state, const MotorFocInput *input, MotorFocOutput *output);

#endif
