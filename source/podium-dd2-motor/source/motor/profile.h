#ifndef OPENTEC_MOTOR_PROFILE_H
#define OPENTEC_MOTOR_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t encoder_period;
    uint32_t encoder_modulus;
    uint32_t position_limit;
    uint32_t position_scale;
    uint32_t velocity_scale;
    uint32_t secondary_scale;
    uint16_t correction_table_length;
    uint8_t adc_auxiliary_channel;
} MotorHardwareProfile;

MotorHardwareProfile motor_hardware_profile_select(bool alternate_hardware);

#endif
