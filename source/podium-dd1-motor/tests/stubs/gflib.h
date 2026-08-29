#ifndef OPENTEC_TEST_GFLIB_H
#define OPENTEC_TEST_GFLIB_H

#include <limits.h>
#include <stdint.h>

typedef int16_t frac16_t;
typedef int32_t frac32_t;
typedef int32_t acc32_t;
typedef uint16_t bool_t;

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

static inline void GFLIB_CtrlPIpAWInit_F16(frac16_t initial, GFLIB_CTRL_PI_P_AW_T_A32 *controller) {
    controller->f32IAccK_1 = (int32_t)initial * 65536;
    controller->f16InErrK_1 = 0;
}

static inline frac32_t GFLIB_Ramp_F32(frac32_t target, GFLIB_RAMP_T_F32 *ramp) {
    int64_t state = ramp->f32State;
    if (target > state) {
        state += ramp->f32RampUp;
        if (state > target) {
            state = target;
        }
    } else if (target < state) {
        state -= ramp->f32RampDown;
        if (state < target) {
            state = target;
        }
    }
    ramp->f32State = (frac32_t)state;
    return ramp->f32State;
}

static inline frac16_t GFLIB_CtrlPIpAW_F16(frac16_t error, const bool_t *stop_integrator,
                                           GFLIB_CTRL_PI_P_AW_T_A32 *controller) {
    (void)stop_integrator;
    controller->f16InErrK_1 = error;
    if (error > controller->f16UpperLim) {
        controller->bLimFlag = 1U;
        return controller->f16UpperLim;
    }
    if (error < controller->f16LowerLim) {
        controller->bLimFlag = 1U;
        return controller->f16LowerLim;
    }
    controller->bLimFlag = 0U;
    return error;
}

static inline frac16_t GFLIB_Sqrt_F16(frac16_t value) { return value < 0 ? 0 : value; }

#endif
