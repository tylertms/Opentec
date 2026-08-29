#ifndef OPENTEC_MOTOR_PERIPHERALS_H
#define OPENTEC_MOTOR_PERIPHERALS_H

#include <stdint.h>

#include "common/motor/calibration.h"

void motor_adc_initialize(void);
void motor_adc_trigger_initialize(void);
void motor_adc_trigger_enable(void);
MotorCurrentCalibrationResult motor_current_calibration_poll(MotorCurrentCalibrationState *state);
void motor_adc_runtime_initialize(uint32_t adc1_auxiliary_channel);
void motor_pwm_initialize(void);
void motor_pwm_enable_outputs(void);
void motor_tick_timer_initialize(uint16_t modulus);

#endif
