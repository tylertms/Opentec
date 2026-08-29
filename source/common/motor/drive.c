#include "common/motor/drive.h"

#include <stdbool.h>
#include <stdint.h>

#include "common/motor/motion.h"

enum {
    FORCE_LIMIT = 65535,
    REDUCED_CONTROLLER_COEFFICIENT = 0x11c7,
    ACTIVE_CONTROLLER_COEFFICIENT = 0x9999,
    CONTROLLER_SCALE = 0x147,
    INTERPOLATION_ERROR_SCALE = 0x666,
    INTERPOLATION_ACCUMULATOR_GAIN = 0x001da12f,
    INTERPOLATION_ACCUMULATOR_OUTPUT_SCALE = 12,
};

static const uint32_t interpolation_coefficients[MOTOR_DRIVE_INTERPOLATION_SETTING_COUNT] = {
    0x322U, 0x333U, 0x343U, 0x353U, 0x374U, 0x395U, 0x3b6U, 0x3d7U, 0x47aU,  0x51eU,
    0x5c2U, 0x666U, 0x70aU, 0x7aeU, 0x8f5U, 0xa3dU, 0xb85U, 0xcccU, 0x1333U, 0x1999U,
};

/**
 * @brief Adds two signed motor current values with sixteen-bit saturation.
 *
 * The fixed-point interpolation stages use the same upper and lower limits as the official
 * saturating addition primitive.
 *
 * @param left First signed current value.
 * @param right Second signed current value.
 * @return Saturated signed sum.
 */
static int16_t add_saturate(int16_t left, int16_t right) {
    int32_t result = (int32_t)left + right;
    if (result > INT16_MAX) {
        return INT16_MAX;
    }
    if (result < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)result;
}

/**
 * @brief Resolves the official live force fields into product-scaled motor current commands.
 *
 * Primary magnitude and signed secondary force are limited, product-scaled, and converted to the
 * current references and FOC coefficients selected by the current status byte.
 *
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

/**
 * @brief Advances the official force interpolation filter.
 *
 * Settings zero through nineteen select the recovered fixed-point response coefficients. Higher
 * settings bypass interpolation, publish the current sample, and clear the dynamic filter state.
 *
 * @param state Persistent interpolation output, error, and accumulator.
 * @param sample Current signed primary drive command.
 * @param setting Interpolation response index from the live motor parameter bank.
 * @return Filtered or bypassed primary drive command.
 */
int16_t motor_drive_interpolation_step(MotorDriveInterpolationState *state, int16_t sample,
                                       uint8_t setting) {
    if (setting >= MOTOR_DRIVE_INTERPOLATION_SETTING_COUNT) {
        state->accumulator = 0U;
        state->output = sample;
        state->error = 0;
        return sample;
    }

    state->output = add_saturate(
        state->output,
        motor_q15_scale_saturate(state->accumulator, INTERPOLATION_ACCUMULATOR_OUTPUT_SCALE));
    state->error = motor_signed_difference_saturate(sample, state->output);
    state->output = add_saturate(
        state->output, motor_q15_scale_saturate(interpolation_coefficients[setting], state->error));
    int16_t accumulator_input = motor_q15_scale_saturate(INTERPOLATION_ERROR_SCALE, state->error);
    int16_t accumulator_step =
        motor_q15_scale_saturate(INTERPOLATION_ACCUMULATOR_GAIN, accumulator_input);
    state->accumulator += (uint32_t)(int32_t)accumulator_step;
    return state->output;
}
