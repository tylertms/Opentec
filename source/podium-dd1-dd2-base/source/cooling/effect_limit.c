#include "cooling/effect_limit.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    COOLING_EFFECT_STRENGTH_LIMIT = 10,
};

static void clamp_strengths(CoolingEffectStrengths *strengths) {
    if (strengths->damper > COOLING_EFFECT_STRENGTH_LIMIT) {
        strengths->damper = COOLING_EFFECT_STRENGTH_LIMIT;
    }
    if (strengths->force > COOLING_EFFECT_STRENGTH_LIMIT) {
        strengths->force = COOLING_EFFECT_STRENGTH_LIMIT;
    }
    if (strengths->spring > COOLING_EFFECT_STRENGTH_LIMIT) {
        strengths->spring = COOLING_EFFECT_STRENGTH_LIMIT;
    }
}

void cooling_effect_limit_init(CoolingEffectLimit *limit) { *limit = (CoolingEffectLimit){0}; }

/**
 * @brief Applies and releases the standard or auxiliary thermal effect-strength limit.
 * @param[in,out] limit Thermal effect-limit phase, snapshot, and active indication.
 * @param[in,out] strengths Current force, spring, and damper tuning strengths.
 * @param[in] controller Cooling phase, thresholds, deadline, and delay configuration.
 * @param[in] motor_temperature_c Current motor temperature in degrees Celsius.
 * @param[in] auxiliary_active True when the auxiliary thermal policy is active.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void cooling_effect_limit_update(CoolingEffectLimit *limit, CoolingEffectStrengths *strengths,
                                 const CoolingController *controller, float motor_temperature_c,
                                 bool auxiliary_active, uint32_t now_ms) {
    switch (limit->phase) {
    case COOLING_EFFECT_LIMIT_INACTIVE:
        if (auxiliary_active) {
            if (controller->phase > COOLING_PHASE_START_AUXILIARY_WINDOW &&
                now_ms > controller->primary_deadline_ms + controller->secondary_delay_ms) {
                limit->phase = COOLING_EFFECT_LIMIT_AUXILIARY;
                limit->snapshot = *strengths;
            }
        } else if (motor_temperature_c > 115.0f) {
            limit->phase = COOLING_EFFECT_LIMIT_STANDARD;
            limit->snapshot = *strengths;
        }
        limit->active = false;
        break;
    case COOLING_EFFECT_LIMIT_STANDARD:
        clamp_strengths(strengths);
        limit->active = true;
        if (motor_temperature_c < 100.0f) {
            limit->phase = COOLING_EFFECT_LIMIT_INACTIVE;
            *strengths = limit->snapshot;
        }
        break;
    case COOLING_EFFECT_LIMIT_AUXILIARY:
        clamp_strengths(strengths);
        limit->active = true;
        if (motor_temperature_c < (float)(controller->low_threshold_offset + 115)) {
            limit->phase = COOLING_EFFECT_LIMIT_INACTIVE;
            *strengths = limit->snapshot;
        }
        break;
    default:
        limit->phase = COOLING_EFFECT_LIMIT_INACTIVE;
        break;
    }
}
