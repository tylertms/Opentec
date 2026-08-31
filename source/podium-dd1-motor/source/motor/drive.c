#include "motor/drive.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/motion.h"

enum {
    FORCE_LIMIT = 65535,
    REDUCED_CONTROLLER_COEFFICIENT = 0x11c7,
    ACTIVE_CONTROLLER_COEFFICIENT = 0x9999,
    CONTROLLER_SCALE = 0x147,
    INTERPOLATION_ERROR_SCALE = 0x666,
    INTERPOLATION_ACCUMULATOR_GAIN = 0x001da12f,
    INTERPOLATION_ACCUMULATOR_OUTPUT_SCALE = 12,
    NATURAL_EFFECT_SCALE = 0x18000,
    FRICTION_EXCURSION_SCALE = 0x190000,
    FRICTION_SETTING_DIVISOR = 0xa0000,
    DERATING_TEMPERATURE_THRESHOLD = 0x507,
    DERATING_TEMPERATURE_SHIFT = 4,
    OVERSPEED_THRESHOLD = 0x2ccc,
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
        .primary_positive = positive,
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

/**
 * @brief Resolves an official natural damping or inertia component.
 *
 * The eight-bit tuning value is converted to Q15, applied to the signed motion sample, and scaled
 * by the shared natural-effect gain before sixteen-bit saturation.
 *
 * @param motion Signed filtered velocity or acceleration sample.
 * @param setting Natural damping or inertia tuning value.
 * @return Signed current component that opposes the supplied motion.
 */
int16_t motor_drive_motion_resistance_resolve(int16_t motion, uint8_t setting) {
    int16_t weighted_motion = motor_q15_scale_wrap((uint32_t)setting << 7U, motion);
    return motor_q15_scale_saturate(NATURAL_EFFECT_SCALE, weighted_motion);
}

/**
 * @brief Initializes the official natural-friction state.
 *
 * The board-selected fixed-point scale determines the retained excursion limit. The corresponding
 * output scale maps that excursion toward the positive Q15 limit.
 *
 * @param state Persistent friction anchor, previous output, and profile-derived scales.
 * @param hardware_scale Board-selected secondary motion scale.
 */
void motor_drive_friction_initialize(MotorDriveFrictionState *state, uint32_t hardware_scale) {
    state->anchor_position = 0;
    state->previous_raw = 0;
    state->excursion_limit =
        (uint32_t)(((uint64_t)hardware_scale * FRICTION_EXCURSION_SCALE) >> 30U);
    state->output_scale = INT16_MAX / state->excursion_limit;
}

/**
 * @brief Advances the official retained-position natural-friction effect.
 *
 * The tuning value limits displacement from a moving anchor. Direction reversals publish one zero
 * sample before the newly signed current is allowed through.
 *
 * @param state Persistent friction anchor, previous output, and profile-derived scales.
 * @param position Current extended encoder position.
 * @param setting Unsigned sixteen-bit natural-friction tuning value.
 * @return Signed current component that opposes retained encoder displacement.
 */
int16_t motor_drive_friction_step(MotorDriveFrictionState *state, int32_t position,
                                  uint16_t setting) {
    if (setting == 0U) {
        return 0;
    }

    int32_t limit = (int32_t)(state->excursion_limit * setting / FRICTION_SETTING_DIVISOR);
    int32_t displacement = position - state->anchor_position;
    if (displacement > limit) {
        state->anchor_position = position - limit;
        displacement = limit;
    } else if (displacement < -limit) {
        state->anchor_position = position + limit;
        displacement = -limit;
    }

    int32_t raw = (int32_t)state->output_scale * displacement;
    bool direction_reversed =
        (state->previous_raw < 0 && raw > 0) || (state->previous_raw > 0 && raw < 0);
    state->previous_raw = raw;
    return direction_reversed ? 0 : (int16_t)raw;
}

/**
 * @brief Initializes the official product-current derating state.
 *
 * Normal output begins at the product maximum. The periodic NXP PI controller subsequently moves
 * this scale toward the target and error published by the product scaling step.
 *
 * @param state Persistent current scale, target, and controller error.
 * @param normal_scale Product-specific normal current scale.
 */
void motor_drive_derating_initialize(MotorDriveDeratingState *state, int16_t normal_scale) {
    state->current_scale = normal_scale;
    state->target_scale = 0;
    state->error = 0;
}

/**
 * @brief Applies the official product-specific current scaling and derating target.
 *
 * Minimum mode uses the product floor directly. Normal mode applies the maximum scale, derives a
 * thermal and command-magnitude target, then applies the current PI-controlled derating scale.
 *
 * @param state Persistent current scale and newly published derating target and error.
 * @param current Saturated current after natural effects are combined.
 * @param motor_temperature_sample Averaged motor-temperature ADC sample.
 * @param normal_scale Product-specific normal current scale.
 * @param minimum_scale Product-specific minimum and special-mode current scale.
 * @param minimum_mode True when parameter thirty-four selects the minimum scale directly.
 * @return Product-scaled signed current.
 */
int16_t motor_drive_product_scale(MotorDriveDeratingState *state, int16_t current,
                                  uint16_t motor_temperature_sample, int16_t normal_scale,
                                  int16_t minimum_scale, bool minimum_mode) {
    if (minimum_mode) {
        return motor_q15_scale_wrap((uint16_t)minimum_scale, current);
    }

    int16_t scaled_current = motor_q15_scale_wrap((uint16_t)normal_scale, current);
    int16_t temperature_scale = normal_scale;
    if (motor_temperature_sample <= DERATING_TEMPERATURE_THRESHOLD) {
        int16_t reduction = (int16_t)((DERATING_TEMPERATURE_THRESHOLD - motor_temperature_sample)
                                      << DERATING_TEMPERATURE_SHIFT);
        temperature_scale = motor_signed_difference_saturate(temperature_scale, reduction);
        if (temperature_scale < minimum_scale) {
            temperature_scale = minimum_scale;
        }
    }

    int16_t magnitude = scaled_current == INT16_MIN
                            ? INT16_MIN
                            : (scaled_current < 0 ? (int16_t)-scaled_current : scaled_current);
    state->target_scale = temperature_scale;
    if (magnitude > minimum_scale) {
        state->target_scale = motor_signed_difference_saturate(
            temperature_scale, (int16_t)(magnitude - minimum_scale));
    }
    state->error = motor_signed_difference_saturate(state->target_scale, state->current_scale);

    int16_t factor = state->current_scale >= normal_scale
                         ? INT16_MAX
                         : (int16_t)(((int32_t)state->current_scale << 15U) / normal_scale);
    return motor_q15_scale_wrap((uint16_t)factor, scaled_current);
}

/**
 * @brief Applies the official permanent over-speed current latch.
 *
 * The first sample outside the positive or negative threshold arms the latch while preserving that
 * sample. Every subsequent sample is replaced by zero until the controller resets.
 *
 * @param state Persistent over-speed latch.
 * @param current Product-scaled signed current.
 * @param velocity Filtered position delta used by the official safety check.
 * @return Original current before latching, otherwise zero after the latch is armed.
 */
int16_t motor_drive_overspeed_apply(MotorDriveOverspeedState *state, int16_t current,
                                    int16_t velocity) {
    if (state->latched) {
        return 0;
    }
    if (velocity > OVERSPEED_THRESHOLD || velocity < -OVERSPEED_THRESHOLD) {
        state->latched = true;
    }
    return current;
}
