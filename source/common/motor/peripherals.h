#ifndef OPENTEC_MOTOR_PERIPHERALS_H
#define OPENTEC_MOTOR_PERIPHERALS_H

#include <stdbool.h>
#include <stdint.h>

#include "common/motor/calibration.h"

typedef void (*MotorTimerHandler)(void *context);
typedef void (*MotorEncoderOverflowHandler)(bool increasing, void *context);
typedef void (*MotorAdcHandler)(int16_t electrical_angle, bool auxiliary_sample_due, void *context);

void motor_adc_initialize(uint32_t encoder_scale, MotorAdcHandler handler, void *context);
void motor_adc_trigger_initialize(void);
void motor_adc_trigger_enable(void);
MotorCurrentCalibrationResult motor_current_calibration_poll(MotorCurrentCalibrationState *state);
void motor_adc_runtime_initialize(uint32_t adc1_auxiliary_channel);
void motor_reset_filter_initialize(void);
void motor_interrupts_initialize(void);
void motor_pwm_initialize(void);
void motor_pwm_enable_outputs(void);
void motor_tick_timer_initialize(uint16_t modulus, MotorEncoderOverflowHandler handler,
                                 void *context);
void motor_service_timer_initialize(MotorTimerHandler handler, void *context);
void motor_communication_timeout_timer_initialize(MotorTimerHandler handler, void *context);

#endif
