#include "motor/profile.h"

/**
 * @brief Selects the motor timing and sensing constants for the detected board variant.
 *
 * The hardware strap chooses one complete set of encoder, sensing, and correction-table values.
 *
 * @param alternate_hardware True when the hardware strap selects the alternate profile.
 * @return Complete motor hardware profile.
 */
MotorHardwareProfile motor_hardware_profile_select(bool alternate_hardware) {
    if (alternate_hardware) {
        return (MotorHardwareProfile){
            .encoder_period = 0x5d2cU,
            .encoder_modulus = 0x5d2bU,
            .position_limit = 0x00028c34U,
            .position_scale = 0x000f1ca2U,
            .velocity_scale = 0x0029374bU,
            .secondary_scale = 0x002120a3U,
            .correction_table_length = 0x0951U,
            .adc_auxiliary_channel = 4U,
        };
    }

    return (MotorHardwareProfile){
        .encoder_period = 0x5c80U,
        .encoder_modulus = 0x5c7fU,
        .position_limit = 0x00028780U,
        .position_scale = 0x000f3851U,
        .velocity_scale = 0x002985a1U,
        .secondary_scale = 0x0020e374U,
        .correction_table_length = 0x0940U,
        .adc_auxiliary_channel = 7U,
    };
}
