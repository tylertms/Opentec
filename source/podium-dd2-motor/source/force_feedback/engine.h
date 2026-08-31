#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_ENGINE_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/effect.h"
#include "force_feedback/soft_stop.h"

/** @brief Number of effect slots in the motor force-feedback engine. */
#define MOTOR_FORCE_FEEDBACK_EFFECT_COUNT 20U

/**
 * @brief Identifies the decoded payload type stored in one force-feedback effect slot.
 *
 * The values match the effect-kind byte used by motor-link configuration commands.
 */
typedef enum {
    MOTOR_FORCE_FEEDBACK_EFFECT_NONE = 0,        /**< No decoded effect payload. */
    MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT = 8,    /**< Decoded constant-force effect. */
    MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW = 11,     /**< Decoded position-window effect. */
    MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL = 12, /**< Decoded directional velocity effect. */
} MotorForceFeedbackEffectType;

/**
 * @brief One configured force-feedback effect slot.
 *
 * The active flag controls whether the decoded payload contributes to the next force mix.
 */
typedef struct {
    MotorForceFeedbackEffectType type; /**< Decoded payload type stored in the slot. */
    bool active;                       /**< Whether the slot contributes to force mixing. */
    /**
     * @brief Payload storage selected by type.
     *
     * Only the union member corresponding to @ref type is meaningful.
     */
    union {
        MotorConstantEffect constant;       /**< Constant-force payload. */
        MotorWindowEffect window;           /**< Position-window payload. */
        MotorDirectionalEffect directional; /**< Directional velocity payload. */
    } data;                                 /**< Decoded payload for the slot. */
} MotorForceFeedbackEffect;

/**
 * @brief Complete state for the motor force-feedback effect mixer.
 *
 * Holds live settings, configured effects, built-in compensation, filtering, travel-limit state,
 * and the recovery ramp used when local effects are enabled.
 */
typedef struct {
    MotorForceFeedbackSettings settings; /**< Live scaling and filter settings. */
    MotorForceFeedbackEffect effects[MOTOR_FORCE_FEEDBACK_EFFECT_COUNT]; /**< Configured slots. */
    MotorDirectionalEffect window_compensation; /**< Internal velocity compensation effect. */
    MotorForceFeedbackFilter filter;             /**< Primary-force moving-average filter. */
    MotorForceFeedbackSoftStop soft_stop;        /**< Travel-limit force state. */
    uint16_t soft_stop_transition_range;         /**< Distance over which soft-stop force ramps. */
    uint8_t ramp_percent;                        /**< Global local-effect recovery percentage. */
} MotorForceFeedbackEngine;

/**
 * @brief Force result produced by one effect-mixer pass.
 *
 * Primary force is converted to direction and magnitude for the motor command; secondary force
 * remains signed until the drive command applies its output policy.
 */
typedef struct {
    MotorForceFeedbackOutput primary; /**< Resolved primary direction and magnitude. */
    int32_t secondary;                /**< Signed secondary force contribution. */
} MotorForceFeedbackMix;

/**
 * @brief Initializes the effect engine with official defaults and built-in effects.
 *
 * Clears all slots, installs the position and damper effects, configures the primary filter, and
 * starts the local-effect recovery ramp at full strength.
 *
 * @param[out] engine Force-feedback engine state to initialize.
 */
void motor_force_feedback_engine_initialize(MotorForceFeedbackEngine *engine);

/**
 * @brief Configures one constant-force effect slot.
 *
 * A valid slot receives the decoded payload and retains its existing activation state.
 *
 * @param[in,out] engine Force-feedback engine to update.
 * @param[in] slot Effect slot from zero through nineteen.
 * @param[in] payload Five-byte constant-force payload.
 * @return True when the slot was valid.
 */
bool motor_force_feedback_constant_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                             const uint8_t payload[5]);

/**
 * @brief Configures one position-window effect slot.
 *
 * A valid payload is decoded against the engine's current steering half-range and retains the
 * slot's existing activation state.
 *
 * @param[in,out] engine Force-feedback engine to update.
 * @param[in] slot Effect slot from zero through nineteen.
 * @param[in] payload Five-byte position-window payload.
 * @return True when the slot was valid.
 */
bool motor_force_feedback_window_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                           const uint8_t payload[5]);

/**
 * @brief Configures one directional velocity-effect slot.
 *
 * A valid slot receives the decoded positive and negative velocity responses without changing
 * activation.
 *
 * @param[in,out] engine Force-feedback engine to update.
 * @param[in] slot Effect slot from zero through nineteen.
 * @param[in] payload Five-byte directional-effect payload.
 * @return True when the slot was valid.
 */
bool motor_force_feedback_directional_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                                const uint8_t payload[5]);

/**
 * @brief Enables one force-feedback effect slot.
 *
 * Only a slot inside the engine's twenty-slot range is changed.
 *
 * @param[in,out] engine Force-feedback engine to update.
 * @param[in] slot Effect slot from zero through nineteen.
 * @return True when the slot was valid.
 */
bool motor_force_feedback_effect_enable(MotorForceFeedbackEngine *engine, uint8_t slot);

/**
 * @brief Disables one force-feedback effect slot.
 *
 * Only a slot inside the engine's twenty-slot range is changed.
 *
 * @param[in,out] engine Force-feedback engine to update.
 * @param[in] slot Effect slot from zero through nineteen.
 * @return True when the slot was valid.
 */
bool motor_force_feedback_effect_disable(MotorForceFeedbackEngine *engine, uint8_t slot);

/**
 * @brief Mixes active effects and applies gain, filtering, ramp, and soft-stop processing.
 *
 * Active slots contribute to primary or secondary accumulators before the output stages produce a
 * force result for the motor drive command.
 *
 * @param[in,out] engine Force-feedback engine state.
 * @param[in] now Current motor service tick.
 * @param[in] centered_position Centered and clamped position used by ordinary effects.
 * @param[in] soft_stop_center Configured encoder center used by the travel-limit effect.
 * @param[in] soft_stop_position Raw extended position used by the travel-limit effect.
 * @param[in] velocity Current signed encoder velocity.
 * @param[in] soft_stop_disabled True when motor status suppresses the travel-limit effect.
 * @return Primary direction and magnitude plus the signed secondary force.
 */
MotorForceFeedbackMix motor_force_feedback_mix(MotorForceFeedbackEngine *engine, uint32_t now,
                                               int32_t centered_position, int32_t soft_stop_center,
                                               int32_t soft_stop_position, int32_t velocity,
                                               bool soft_stop_disabled);

#endif
