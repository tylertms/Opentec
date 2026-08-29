#ifndef OPENTEC_MOTOR_PRODUCT_H
#define OPENTEC_MOTOR_PRODUCT_H

#include <stdint.h>

typedef struct {
    uint8_t normal_output_percent;
} MotorProductConfiguration;

extern const MotorProductConfiguration motor_product_configuration;

#endif
