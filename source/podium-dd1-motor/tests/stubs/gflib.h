#ifndef VELOCITY_HOST_STUB_GFLIB_H
#define VELOCITY_HOST_STUB_GFLIB_H

#include "mlib.h"

typedef uint8_t bool_t;
typedef int32_t acc32_t;

typedef struct {
    acc32_t a32PGain;
    acc32_t a32IGain;
    frac32_t f32IAccK_1;
    frac16_t f16InErrK_1;
    frac16_t f16UpperLim;
    frac16_t f16LowerLim;
    bool_t bLimFlag;
} GFLIB_CTRL_PI_P_AW_T_A32;

typedef struct {
    frac32_t f32RampUp;
    frac32_t f32RampDown;
    frac32_t f32State;
} GFLIB_RAMP_T_F32;

static inline void GFLIB_CtrlPIpAWInit_F16(frac16_t value, GFLIB_CTRL_PI_P_AW_T_A32 *controller) {
    controller->f32IAccK_1 = MLIB_Conv_F32s(value);
    controller->f16InErrK_1 = 0;
}

static inline frac32_t GFLIB_Ramp_F32(frac32_t target, GFLIB_RAMP_T_F32 *ramp) {
    ramp->f32State = target;
    return target;
}

static inline frac16_t GFLIB_CtrlPIpAW_F16(frac16_t error, const bool_t *stop_integrator,
                                           GFLIB_CTRL_PI_P_AW_T_A32 *controller) {
    (void)error;
    (void)stop_integrator;
    (void)controller;
    return 0;
}

#endif
