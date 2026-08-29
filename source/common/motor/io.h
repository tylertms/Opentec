#ifndef OPENTEC_MOTOR_IO_H
#define OPENTEC_MOTOR_IO_H

#include <fsl_device_registers.h>
#include <gmclib.h>
#include <stdbool.h>

#include "common/motor/calibration.h"

typedef struct {
    GMCLIB_3COOR_T_F16 phase_current;
    frac16_t dc_bus_voltage;
} MotorAdcSample;

bool motor_adc_read(ADC_Type *adc0, ADC_Type *adc1, const MotorCurrentOffsets *offsets,
                    MotorAdcSample *sample);
void motor_pwm_write(FTM_Type *ftm, GMCLIB_3COOR_T_F16 *duty);

#endif
