#include "motor/pi.h"

#include <stdint.h>
#if defined(__GNUC__)
#pragma GCC optimize("O2")
#endif

static inline int32_t motor_pi_signed(uint32_t bits) { return (int32_t)bits; }

static inline int32_t motor_pi_q16(int64_t value) {
    return motor_pi_signed((uint32_t)((uint64_t)value >> 16U));
}

frac16_t motor_pi_step(frac16_t error, const bool_t *stop_integrator,
                       GFLIB_CTRL_PI_P_AW_T_A32 *controller) {
    int32_t integration_error = controller->f16InErrK_1;
    if (*stop_integrator == 0U) {
        integration_error += error;
    }

    int64_t integral = (int64_t)integration_error * controller->a32IGain + controller->f32IAccK_1;
    uint32_t integral_bits = (uint32_t)((uint64_t)integral >> 16U);
    if (((uint32_t)integral & UINT32_C(0xffff)) != 0U) {
        ++integral_bits;
    }
    int32_t integral_value = motor_pi_signed(integral_bits);
    if (integral_value > controller->f16UpperLim) {
        controller->f32IAccK_1 = (int32_t)controller->f16UpperLim * INT32_C(65536);
    } else if (integral_value < controller->f16LowerLim) {
        controller->f32IAccK_1 = (int32_t)controller->f16LowerLim * INT32_C(65536);
    } else {
        controller->f32IAccK_1 = motor_pi_signed((uint32_t)integral);
    }
    controller->f16InErrK_1 = error;

    int64_t output_accumulator =
        (int64_t)error * controller->a32PGain * INT64_C(2) + controller->f32IAccK_1;
    int32_t output = motor_pi_q16(output_accumulator);
    controller->bLimFlag = 0U;
    if (output >= controller->f16UpperLim) {
        output = controller->f16UpperLim;
        controller->bLimFlag = 1U;
    }
    if (output <= controller->f16LowerLim) {
        output = controller->f16LowerLim;
        controller->bLimFlag = 1U;
    }
    return (frac16_t)output;
}
