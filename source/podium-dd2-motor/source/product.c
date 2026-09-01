#include "product.h"

/**
 * @brief Scaling constants for the Podium DD2 motor product.
 */
const MotorProductConfiguration motor_product_configuration = {
    .normal_output_percent = 40U,
    .normal_current_scale = 0x770a,
    .minimum_current_scale = 0x5c28,
    .torque_telemetry_scale = 0x6f5cU,
};
