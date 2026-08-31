#ifndef OPENTEC_MOTOR_PRODUCT_H
#define OPENTEC_MOTOR_PRODUCT_H

#include <stdint.h>

typedef struct {
    uint8_t normal_output_percent;
    int16_t normal_current_scale;
    int16_t minimum_current_scale;
    uint16_t torque_telemetry_scale;
} MotorProductConfiguration;

extern const MotorProductConfiguration motor_product_configuration;

#endif
