#include "force_feedback/engine.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/** @brief Force-mixing limits and travel-limit transition configuration. */
enum {
    PRIMARY_FORCE_LIMIT = 65535,       /**< Maximum absolute primary force accumulator. */
    SECONDARY_FORCE_LIMIT = 32767,     /**< Maximum absolute secondary force accumulator. */
    SOFT_STOP_TRANSITION_RANGE = 0x19b1, /**< Travel distance for soft-stop force interpolation. */
};

/**
 * @brief Limits the signed primary force accumulator.
 *
 * The recovered force mixer uses symmetric positive and negative limits of 65535.
 *
 * @param[in] force Signed accumulated primary force.
 * @return Primary force limited to the official range.
 */
static int32_t clamp_primary(int32_t force) {
    if (force > PRIMARY_FORCE_LIMIT) {
        return PRIMARY_FORCE_LIMIT;
    }
    if (force < -PRIMARY_FORCE_LIMIT) {
        return -PRIMARY_FORCE_LIMIT;
    }
    return force;
}

/**
 * @brief Limits the signed secondary force accumulator.
 *
 * The secondary path uses symmetric positive and negative limits of 32767.
 *
 * @param[in] force Signed accumulated secondary force.
 * @return Secondary force limited to the official range.
 */
static int32_t clamp_secondary(int32_t force) {
    if (force > SECONDARY_FORCE_LIMIT) {
        return SECONDARY_FORCE_LIMIT;
    }
    if (force < -SECONDARY_FORCE_LIMIT) {
        return -SECONDARY_FORCE_LIMIT;
    }
    return force;
}

/**
 * @brief Applies the live overall force gain.
 *
 * Zero suppresses output and one hundred bypasses division.
 *
 * @param[in] force Signed force before overall scaling.
 * @param[in] gain_percent Overall force gain from zero through one hundred.
 * @return Scaled signed force.
 */
static int32_t apply_overall_gain(int32_t force, uint8_t gain_percent) {
    if (gain_percent == 0U) {
        return 0;
    }
    return gain_percent == 100U ? force : force * gain_percent / 100;
}

void motor_force_feedback_engine_initialize(MotorForceFeedbackEngine *engine) {
    *engine = (MotorForceFeedbackEngine){0};
    engine->settings = motor_force_feedback_settings_default();
    engine->ramp_percent = 100U;
    engine->soft_stop_transition_range = SOFT_STOP_TRANSITION_RANGE;
    motor_force_feedback_filter_configure(&engine->filter, engine->settings.filter_setting);

    const uint8_t position_payload[5] = {0x80U, 0x80U, 0x44U, 0U, 0xffU};
    motor_force_feedback_window_configure(engine, MOTOR_FORCE_FEEDBACK_POSITION_SLOT,
                                          position_payload);
    motor_force_feedback_effect_enable(engine, MOTOR_FORCE_FEEDBACK_POSITION_SLOT);

    engine->window_compensation = (MotorDirectionalEffect){
        .positive_coefficient = 1U,
        .negative_coefficient = 1U,
        .positive_direction = 1,
        .negative_direction = 1,
        .saturation = UINT16_MAX,
    };

    const uint8_t damper_payload[5] = {1U, 0U, 1U, 0U, 0xa0U};
    motor_force_feedback_directional_configure(engine, MOTOR_FORCE_FEEDBACK_DAMPER_SLOT,
                                               damper_payload);
    motor_force_feedback_effect_enable(engine, MOTOR_FORCE_FEEDBACK_DAMPER_SLOT);
}

bool motor_force_feedback_constant_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                             const uint8_t payload[5]) {
    if (slot >= MOTOR_FORCE_FEEDBACK_EFFECT_COUNT) {
        return false;
    }
    engine->effects[slot].type = MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT;
    engine->effects[slot].data.constant = motor_force_feedback_constant_decode(payload);
    return true;
}

bool motor_force_feedback_window_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                           const uint8_t payload[5]) {
    if (slot >= MOTOR_FORCE_FEEDBACK_EFFECT_COUNT) {
        return false;
    }
    engine->effects[slot].type = MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW;
    engine->effects[slot].data.window =
        motor_force_feedback_window_decode(payload, engine->settings.position_half_range);
    return true;
}

bool motor_force_feedback_directional_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                                const uint8_t payload[5]) {
    if (slot >= MOTOR_FORCE_FEEDBACK_EFFECT_COUNT) {
        return false;
    }
    bool steering_scaled = engine->effects[slot].data.window.lower_direction != 0;
    engine->effects[slot].type = MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL;
    engine->effects[slot].data.directional = motor_force_feedback_directional_decode(payload);
    engine->effects[slot].data.directional.steering_scaled = steering_scaled;
    return true;
}

bool motor_force_feedback_effect_enable(MotorForceFeedbackEngine *engine, uint8_t slot) {
    if (slot >= MOTOR_FORCE_FEEDBACK_EFFECT_COUNT) {
        return false;
    }
    engine->effects[slot].active = true;
    return true;
}

bool motor_force_feedback_effect_disable(MotorForceFeedbackEngine *engine, uint8_t slot) {
    if (slot >= MOTOR_FORCE_FEEDBACK_EFFECT_COUNT) {
        return false;
    }
    engine->effects[slot].active = false;
    return true;
}

MotorForceFeedbackMix motor_force_feedback_mix(MotorForceFeedbackEngine *engine, uint32_t now,
                                               int32_t centered_position, int32_t soft_stop_center,
                                               int32_t soft_stop_position, int32_t velocity,
                                               bool soft_stop_disabled) {
    int32_t primary = 0;
    int32_t secondary = 0;
    for (uint8_t slot = 0U; slot < MOTOR_FORCE_FEEDBACK_EFFECT_COUNT; ++slot) {
        const MotorForceFeedbackEffect *effect = &engine->effects[slot];
        if (!effect->active) {
            continue;
        }
        if (effect->type == MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT) {
            primary += motor_force_feedback_constant_evaluate(
                &effect->data.constant, engine->settings.constant_gain_tenths);
        } else if (effect->type == MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW) {
            primary += motor_force_feedback_window_evaluate(
                &effect->data.window, centered_position, velocity,
                engine->settings.window_multiplier, engine->settings.window_gain_tenths, slot,
                &engine->window_compensation, engine->settings.directional_gain_tenths,
                engine->settings.overall_gain_percent);
        } else if (effect->type == MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL) {
            secondary += motor_force_feedback_directional_evaluate(
                &effect->data.directional, velocity, engine->settings.directional_gain_tenths,
                engine->settings.overall_gain_percent, slot);
        }
    }

    primary = clamp_primary(apply_overall_gain(primary, engine->settings.overall_gain_percent));
    secondary =
        clamp_secondary(apply_overall_gain(secondary, engine->settings.overall_gain_percent));
    motor_force_feedback_filter_configure(&engine->filter, engine->settings.filter_setting);
    primary = motor_force_feedback_filter_apply(&engine->filter, primary);
    primary = primary * engine->ramp_percent / 100;
    secondary = secondary * engine->ramp_percent / 100;

    bool damper_active = motor_force_feedback_soft_stop_apply(
        &engine->soft_stop, now, engine->settings.position_half_range, soft_stop_center,
        soft_stop_position, engine->soft_stop_transition_range, soft_stop_disabled, &primary);
    engine->effects[MOTOR_FORCE_FEEDBACK_DAMPER_SLOT].active = damper_active;

    return (MotorForceFeedbackMix){
        .primary = motor_force_feedback_output_resolve(primary),
        .secondary = secondary,
    };
}
