#include "cooling/effect_limit.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    COOLING_EFFECT_STRENGTH_LIMIT = 10,
};

/**
 * @brief Clamps all thermally managed effect strengths.
 *
 * Reduces force, spring, and damper strengths above ten to ten and leaves lower values unchanged.
 *
 * @param[in,out] strengths Effect strengths to constrain.
 */
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

/**
 * @brief Initializes the thermal effect-strength limit.
 *
 * Starts in the inactive phase with no saved strengths and no active limit indication.
 *
 * @param[out] limit Effect-limit state to initialize.
 */
void cooling_effect_limit_init(CoolingEffectLimit *limit) { *limit = (CoolingEffectLimit){0}; }

/**
 * @brief Applies and releases the standalone or managed-motor thermal effect-strength limit.
 *
 * Captures the current strengths when a limit starts, constrains them on subsequent updates, and
 * restores the captured values when the applicable release threshold is crossed.
 *
 * @param[in,out] limit Thermal effect-limit phase, snapshot, and active indication.
 * @param[in,out] strengths Current force, spring, and damper tuning strengths.
 * @param[in] controller Cooling phase, thresholds, deadline, and delay configuration.
 * @param[in] motor_temperature_c Current motor temperature in degrees Celsius.
 * @param[in] managed_motor_present True after a motor controller is identified.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void cooling_effect_limit_update(CoolingEffectLimit *limit, CoolingEffectStrengths *strengths,
                                 const CoolingController *controller, float motor_temperature_c,
                                 bool managed_motor_present, uint32_t now_ms) {
    switch (limit->phase) {
    case COOLING_EFFECT_LIMIT_INACTIVE:
        if (managed_motor_present) {
            if (controller->phase > COOLING_PHASE_START_MANAGED_WINDOW &&
                now_ms > controller->primary_deadline_ms + controller->secondary_delay_ms) {
                limit->phase = COOLING_EFFECT_LIMIT_MANAGED;
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
    case COOLING_EFFECT_LIMIT_MANAGED:
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
