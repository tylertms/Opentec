#include "common/motor/drive.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    FORCE_LIMIT = 65535,
    REDUCED_CONTROLLER_COEFFICIENT = 0x11c7,
    ACTIVE_CONTROLLER_COEFFICIENT = 0x9999,
    CONTROLLER_SCALE = 0x147,
};

/**
 * @brief Resolves the official live force fields into product-scaled motor current commands.
 * @param positive Primary force direction flag.
 * @param primary Primary force magnitude.
 * @param secondary Signed secondary force.
 * @param normal_output_percent Product output scale outside full-torque mode.
 * @param full_torque True when status bit seven bypasses the product output scale.
 * @param reduced_controller True when status selects the reduced controller coefficient.
 * @param secondary_disabled True when status suppresses the secondary current.
 * @return Signed current commands and the selected controller coefficients.
 */
MotorDriveCommand motor_drive_command_resolve(bool positive, uint32_t primary, int32_t secondary,
                                              uint8_t normal_output_percent, bool full_torque,
                                              bool reduced_controller, bool secondary_disabled) {
    if (primary > FORCE_LIMIT) {
        primary = FORCE_LIMIT;
    }
    if (secondary > FORCE_LIMIT) {
        secondary = FORCE_LIMIT;
    }
    if (!full_torque) {
        primary = primary * normal_output_percent / 100U;
        secondary = secondary * normal_output_percent / 100;
    }

    int16_t primary_current = (int16_t)(primary >> 1U);
    if (!positive) {
        primary_current = (int16_t)-primary_current;
    }

    return (MotorDriveCommand){
        .primary_current = primary_current,
        .secondary_current = secondary_disabled ? 0 : (int16_t)secondary,
        .controller_coefficient = primary == 0U || reduced_controller
                                      ? REDUCED_CONTROLLER_COEFFICIENT
                                      : ACTIVE_CONTROLLER_COEFFICIENT,
        .controller_scale = CONTROLLER_SCALE,
    };
}
