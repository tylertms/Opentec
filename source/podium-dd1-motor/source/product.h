#ifndef OPENTEC_MOTOR_PRODUCT_H
#define OPENTEC_MOTOR_PRODUCT_H

#include <stdint.h>

/**
 * @brief Product-specific motor scaling configuration.
 *
 * The runtime uses these values for normal force output, current derating, and torque telemetry.
 */
typedef struct {
    uint8_t normal_output_percent; /**< Normal force output percentage outside full-torque mode. */
    int16_t normal_current_scale; /**< Upper current scale for normal operation. */
    int16_t minimum_current_scale; /**< Lower current scale used by thermal derating. */
    uint16_t torque_telemetry_scale; /**< Scale applied when publishing measured torque. */
} MotorProductConfiguration;

/**
 * @brief Scaling constants selected for the current motor product.
 */
extern const MotorProductConfiguration motor_product_configuration;

#endif
