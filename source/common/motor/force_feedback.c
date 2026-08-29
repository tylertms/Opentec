#include "common/motor/force_feedback.h"

#include <limits.h>
#include <stdint.h>

static int32_t signed_from_uint32(uint32_t value) {
    if (value <= INT32_MAX) {
        return (int32_t)value;
    }
    return -1 - (int32_t)(UINT32_MAX - value);
}

static int32_t axis_decode(uint16_t value) {
    return signed_from_uint32((0x8000U - value) * 2U - 1U);
}

static int32_t clamp_symmetric(int32_t value, uint32_t limit) {
    if (value > (int32_t)limit) {
        return (int32_t)limit;
    }
    if (value < -(int32_t)limit) {
        return -(int32_t)limit;
    }
    return value;
}

/**
 * @brief Returns the force-feedback defaults installed by both official motor images.
 * @return Default motor effect scaling and filter settings.
 */
MotorForceFeedbackSettings motor_force_feedback_settings_default(void) {
    return (MotorForceFeedbackSettings){
        .position_half_range = 0x8ac0,
        .overall_gain_percent = 35U,
        .filter_setting = 100U,
        .constant_gain_tenths = 10U,
        .window_gain_tenths = 10U,
        .directional_gain_tenths = 10U,
        .window_multiplier = 1U,
    };
}

/**
 * @brief Decodes the two official constant-force axis encodings.
 * @param payload Five-byte constant-force payload following effect kind 8.
 * @return Decoded signed force magnitude.
 */
MotorConstantEffect motor_force_feedback_constant_decode(const uint8_t payload[5]) {
    MotorConstantEffect effect = {0};
    if (payload[4] == 0U) {
        if (payload[0] != 0x80U) {
            effect.magnitude = axis_decode((uint16_t)payload[0] * 0x101U);
        }
    } else if (payload[4] == 1U) {
        effect.magnitude = axis_decode((uint16_t)payload[0] | (uint16_t)payload[1] << 8U);
    }
    return effect;
}

/**
 * @brief Decodes the official position-window effect payload.
 * @param payload Five-byte position-window payload following effect kind 11.
 * @param position_half_range Signed position scale used by the motor image.
 * @return Decoded position-window effect.
 */
MotorWindowEffect motor_force_feedback_window_decode(const uint8_t payload[5],
                                                     int32_t position_half_range) {
    return (MotorWindowEffect){
        .lower_position = position_half_range * 2 * payload[0] / 256 - position_half_range,
        .upper_position = position_half_range * 2 * payload[1] / 256 - position_half_range,
        .lower_coefficient = payload[2] >> 4U,
        .upper_coefficient = payload[2] & 0x0fU,
        .lower_direction = (payload[3] & 1U) != 0U ? 1 : -1,
        .upper_direction = (payload[3] & 0x10U) != 0U ? 1 : -1,
        .saturation = (uint16_t)payload[4] * 0x101U,
    };
}

/**
 * @brief Decodes the official two-direction velocity effect payload.
 * @param payload Five-byte velocity-effect payload following effect kind 12.
 * @return Decoded directional effect.
 */
MotorDirectionalEffect motor_force_feedback_directional_decode(const uint8_t payload[5]) {
    return (MotorDirectionalEffect){
        .positive_coefficient = payload[0],
        .negative_coefficient = payload[2],
        .positive_direction = (payload[1] & 1U) != 0U ? 1 : -1,
        .negative_direction = (payload[3] & 1U) != 0U ? 1 : -1,
        .saturation = (uint16_t)payload[4] * 0x101U,
    };
}

/**
 * @brief Applies the official tenths-scale gain to a constant-force effect.
 * @param effect Constant-force effect to evaluate.
 * @param gain_tenths Gain where ten selects the unscaled magnitude.
 * @return Scaled constant-force contribution.
 */
int32_t motor_force_feedback_constant_evaluate(const MotorConstantEffect *effect,
                                               uint8_t gain_tenths) {
    return gain_tenths == 0U ? 0 : effect->magnitude * gain_tenths / 10;
}

/**
 * @brief Evaluates the official signed velocity-condition transfer function.
 * @param effect Directional velocity effect to evaluate.
 * @param velocity Signed motor velocity sample.
 * @param gain_tenths Normal-effect gain where ten is unity.
 * @param overall_gain_percent Overall effect strength used by steering-scaled effects.
 * @param slot Effect slot number.
 * @return Signed velocity-effect contribution.
 */
int32_t motor_force_feedback_directional_evaluate(const MotorDirectionalEffect *effect,
                                                  int32_t velocity, uint8_t gain_tenths,
                                                  uint8_t overall_gain_percent, uint8_t slot) {
    int32_t half_velocity = velocity / 2;
    int32_t force;
    if (half_velocity < 1) {
        force = effect->negative_direction * half_velocity * effect->negative_coefficient / 16;
    } else {
        force = effect->positive_direction * half_velocity * effect->positive_coefficient / 16;
    }
    force = clamp_symmetric(force, effect->saturation);

    if (!effect->steering_scaled && slot != MOTOR_FORCE_FEEDBACK_DAMPER_SLOT) {
        return gain_tenths == 0U ? 0 : force * gain_tenths / 10;
    }

    force = force * 7 / 10;
    return overall_gain_percent == 0U ? force * 100 : force * 100 / overall_gain_percent;
}

/**
 * @brief Evaluates the official position-window transfer and its internal velocity compensation.
 * @param effect Position-window effect to evaluate.
 * @param position Signed motor position sample.
 * @param velocity Signed motor velocity sample.
 * @param multiplier Position-window coefficient multiplier.
 * @param gain_tenths Normal window-effect gain where ten is unity.
 * @param slot Effect slot number.
 * @param internal_effect Internal directional compensation applied by every window effect.
 * @param directional_gain_tenths Normal directional-effect gain where ten is unity.
 * @param overall_gain_percent Overall effect strength used by steering-scaled effects.
 * @return Signed position-window contribution including velocity compensation.
 */
int32_t motor_force_feedback_window_evaluate(const MotorWindowEffect *effect, int32_t position,
                                             int32_t velocity, uint8_t multiplier,
                                             uint8_t gain_tenths, uint8_t slot,
                                             const MotorDirectionalEffect *internal_effect,
                                             uint8_t directional_gain_tenths,
                                             uint8_t overall_gain_percent) {
    int32_t force = 0;
    if (position < effect->lower_position) {
        force = effect->lower_direction * multiplier * effect->lower_coefficient *
                (position - effect->lower_position);
    } else if (position > effect->upper_position) {
        force = effect->upper_direction * multiplier * effect->upper_coefficient *
                (position - effect->upper_position);
    }
    force = clamp_symmetric(force / 5, effect->saturation);
    if (slot != MOTOR_FORCE_FEEDBACK_POSITION_SLOT) {
        force = gain_tenths == 0U ? 0 : force * gain_tenths / 10;
    }
    return force + motor_force_feedback_directional_evaluate(internal_effect, velocity,
                                                             directional_gain_tenths,
                                                             overall_gain_percent, 0U);
}

/**
 * @brief Maps the official force-filter setting to its moving-average window length.
 * @param setting Filter setting from zero through one hundred.
 * @return Moving-average sample count.
 */
uint8_t motor_force_feedback_filter_length(uint8_t setting) {
    switch (setting) {
    case 0U:
        return 40U;
    case 10U:
        return 35U;
    case 20U:
        return 30U;
    case 30U:
        return 25U;
    case 40U:
        return 20U;
    case 50U:
        return 15U;
    case 60U:
        return 10U;
    case 70U:
        return 7U;
    case 80U:
        return 4U;
    case 90U:
        return 2U;
    default:
        return 1U;
    }
}

/**
 * @brief Configures and clears the official force moving-average window.
 * @param filter Moving-average state to configure.
 * @param setting Filter setting from zero through one hundred.
 */
void motor_force_feedback_filter_configure(MotorForceFeedbackFilter *filter, uint8_t setting) {
    if (filter->initialized && filter->setting == setting) {
        return;
    }

    filter->setting = setting;
    filter->length = motor_force_feedback_filter_length(setting);
    filter->index = 0U;
    filter->sum = 0;
    for (uint8_t index = 0U; index < filter->length; ++index) {
        filter->samples[index] = 0;
    }
    filter->initialized = true;
}

/**
 * @brief Advances the official force moving-average filter by one service tick.
 * @param filter Configured moving-average state.
 * @param force New signed force sample.
 * @return Average across the complete configured window.
 */
int32_t motor_force_feedback_filter_apply(MotorForceFeedbackFilter *filter, int32_t force) {
    filter->sum -= filter->samples[filter->index];
    filter->samples[filter->index] = force;
    filter->sum += force;
    ++filter->index;
    if (filter->index >= filter->length) {
        filter->index = 0U;
    }
    return filter->sum / filter->length;
}

/**
 * @brief Converts the official signed primary force into direction and magnitude fields.
 * @param force Signed primary force after filtering and safety compensation.
 * @return Direction flag and magnitude clamped to 65535.
 */
MotorForceFeedbackOutput motor_force_feedback_output_resolve(int32_t force) {
    MotorForceFeedbackOutput output = {.positive = force >= 0};
    uint32_t magnitude = force < 0 ? 0U - (uint32_t)force : (uint32_t)force;
    output.magnitude = magnitude > UINT16_MAX ? UINT16_MAX : (uint16_t)magnitude;
    return output;
}
