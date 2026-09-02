#ifndef OPENTEC_BASE_COOLING_EFFECT_LIMIT_H
#define OPENTEC_BASE_COOLING_EFFECT_LIMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "cooling/controller.h"

/**
 * @brief Force-feedback effect strengths subject to thermal limiting.
 *
 * Values are expressed in the protocol's strength units and are clamped by the effect-limit
 * controller while a thermal limit is active.
 */
typedef struct {
    uint8_t force;  /**< Force-effect strength. */
    uint8_t spring; /**< Spring-effect strength. */
    uint8_t damper; /**< Damper-effect strength. */
} CoolingEffectStrengths;

/**
 * @brief Thermal effect-limit phases.
 *
 * The phase records whether no limit, a standard temperature limit, or a managed-motor limit is
 * currently being applied.
 */
typedef enum {
    COOLING_EFFECT_LIMIT_INACTIVE, /**< No thermal effect limit is active. */
    COOLING_EFFECT_LIMIT_STANDARD, /**< Standard-motor thermal limit is active. */
    COOLING_EFFECT_LIMIT_MANAGED,  /**< Managed-motor thermal limit is active. */
} CoolingEffectLimitPhase;

/**
 * @brief Stateful thermal effect-strength limiter.
 *
 * Stores the current limit phase, the pre-limit strengths to restore, and the active effect-limit
 * latch.
 */
typedef struct {
    CoolingEffectLimitPhase phase;   /**< Current thermal limit phase. */
    CoolingEffectStrengths snapshot; /**< Strengths captured when the current limit began. */
    bool
        active; /**< True while the effect-limit latch remains set, including its release update. */
} CoolingEffectLimit;

/**
 * @brief Initializes the thermal effect-strength limiter.
 *
 * Clears the phase, saved strengths, and active indication so no effect limit is applied.
 *
 * @param[out] limit Effect-limit state to initialize.
 */
void cooling_effect_limit_init(CoolingEffectLimit *limit);

/**
 * @brief Applies or releases a standard or managed-motor effect limit.
 *
 * Captures strengths when a limit begins, clamps subsequent limited updates to the thermal ceiling,
 * and restores the captured strengths after the corresponding release threshold is crossed.
 *
 * @param[in,out] limit Thermal effect-limit phase, snapshot, and active indication.
 * @param[in,out] strengths Current force, spring, and damper strengths to constrain or restore.
 * @param[in] controller Cooling phase, thresholds, deadlines, and delay configuration.
 * @param[in] motor_temperature_c Current motor temperature in degrees Celsius.
 * @param[in] managed_motor_present True after a motor controller is identified.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void cooling_effect_limit_update(CoolingEffectLimit *limit, CoolingEffectStrengths *strengths,
                                 const CoolingController *controller, float motor_temperature_c,
                                 bool managed_motor_present, uint32_t now_ms);

/**
 * @brief Reports the official resistance-profile effect-limit state.
 *
 * Exposes the effect-limit latch used by the Xbox extended-status resistance-profile bit. The
 * latch remains active through the release update and clears on the following inactive update.
 *
 * @param[in] limit Effect-limit state to inspect.
 * @return True while the official resistance-profile effect limit is active.
 */
bool cooling_effect_limit_resistance_profile_active(const CoolingEffectLimit *limit);

#endif
