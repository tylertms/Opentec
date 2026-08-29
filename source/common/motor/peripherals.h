#ifndef OPENTEC_MOTOR_PERIPHERALS_H
#define OPENTEC_MOTOR_PERIPHERALS_H

#include <stdbool.h>
#include <stdint.h>

#include "common/motor/calibration.h"

typedef void (*MotorTimerHandler)(void *context);
typedef void (*MotorEncoderOverflowHandler)(bool increasing, void *context);
typedef void (*MotorEncoderIndexHandler)(uint16_t counter, void *context);
typedef void (*MotorAdcHandler)(int16_t electrical_angle, bool control_update_due, void *context);

typedef struct {
    uint16_t motor;
    uint16_t driver;
} MotorAdcAuxiliarySamples;

void motor_adc_initialize(uint32_t encoder_scale, MotorAdcHandler handler, void *context);
void motor_adc_trigger_initialize(void);
void motor_adc_trigger_enable(void);
void motor_current_calibration_hardware_start(void);
MotorCurrentCalibrationResult motor_current_calibration_poll(MotorCurrentCalibrationState *state);
void motor_adc_runtime_initialize(uint32_t adc0_auxiliary_channel);
bool motor_adc_auxiliary_cycle(MotorAdcAuxiliarySamples *samples);
void motor_reset_filter_initialize(void);
void motor_interrupts_initialize(void);
void motor_pwm_initialize(void);
void motor_pwm_enable_outputs(void);
void motor_tick_timer_initialize(uint16_t modulus, MotorEncoderOverflowHandler handler,
                                 MotorEncoderIndexHandler index_handler, void *context);
void motor_encoder_overflow_interrupt_enable(void);
void motor_encoder_revolution_arm(void);
void motor_encoder_revolution_clear(void);
bool motor_encoder_revolution_is_complete(void);
void motor_encoder_index_interrupt_enable(void);
void motor_encoder_index_interrupt_disable(void);
void motor_service_timer_initialize(MotorTimerHandler handler, void *context);
void motor_communication_timeout_timer_initialize(MotorTimerHandler handler, void *context);

#endif
