#ifndef OPENTEC_BASE_FORCE_FEEDBACK_STATE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/command.h"

/** @brief Total effect-state slots, including the built-in position effect. */
enum {
    FORCE_FEEDBACK_STATE_EFFECT_COUNT =
        FORCE_FEEDBACK_EFFECT_SLOT_COUNT + 1 /**< Host and built-in slot count. */
};

/**
 * @brief Kind of force-feedback effect stored in a state slot.
 *
 * The kind identifies which nested effect configuration retains the decoded parameters for an
 * active slot.
 */
typedef enum {
    FORCE_FEEDBACK_EFFECT_NONE,   /**< Slot has no configured effect kind. */
    FORCE_FEEDBACK_EFFECT_KIND_1, /**< Slot contains a kind-1 effect. */
    FORCE_FEEDBACK_EFFECT_KIND_2, /**< Slot contains a kind-2 effect. */
    FORCE_FEEDBACK_EFFECT_KIND_3, /**< Slot contains a kind-3 effect. */
} ForceFeedbackEffectKind;

/**
 * @brief Configuration for a kind-1 force-feedback effect.
 *
 * The magnitude stores the signed value decoded from the kind-1 command payload.
 */
typedef struct {
    int32_t magnitude; /**< Signed kind-1 effect magnitude. */
} ForceFeedbackKind1Effect;

/**
 * @brief Configuration for a kind-2 force-feedback effect.
 *
 * The two protocol-axis positions, modes, directions, and strength are retained after command
 * decoding; positions are stored on the centered wheel-position scale.
 */
typedef struct {
    int32_t positions[2];  /**< Centered effect positions for the two protocol axes. */
    uint8_t axis_modes[2]; /**< Axis modes for the two protocol axes. */
    int8_t directions[2];  /**< Force directions for the two protocol axes. */
    uint16_t strength;     /**< Effect strength. */
} ForceFeedbackKind2Effect;

/**
 * @brief Configuration for a kind-3 force-feedback effect.
 *
 * The mode, axis mode, directions, and strength are retained as decoded from the host command.
 */
typedef struct {
    uint8_t mode;         /**< Kind-3 effect mode. */
    uint8_t axis_mode;    /**< Kind-3 axis mode. */
    int8_t directions[2]; /**< Force directions for the two protocol axes. */
    uint16_t strength;    /**< Effect strength. */
} ForceFeedbackKind3Effect;

/**
 * @brief Runtime state for one force-feedback effect slot.
 *
 * The active flag controls whether the slot is active while the nested structures retain the
 * parameters for each supported effect kind.
 */
typedef struct {
    ForceFeedbackEffectKind kind;    /**< Selected effect configuration kind. */
    bool active;                     /**< Whether this slot currently contributes force. */
    ForceFeedbackKind1Effect kind_1; /**< Kind-1 effect configuration. */
    ForceFeedbackKind2Effect kind_2; /**< Kind-2 effect configuration. */
    ForceFeedbackKind3Effect kind_3; /**< Kind-3 effect configuration. */
} ForceFeedbackEffectState;

/**
 * @brief Complete host and built-in force-feedback state.
 *
 * Stores all host-controlled effect slots, the built-in position effect slot, and the independent
 * primary and secondary output gates.
 */
typedef struct {
    ForceFeedbackEffectState
        effects[FORCE_FEEDBACK_STATE_EFFECT_COUNT]; /**< Host and built-in effect slots. */
    bool primary_output_disabled;   /**< Whether the primary force output is disabled. */
    bool secondary_output_disabled; /**< Whether the secondary force output is disabled. */
} ForceFeedbackState;

/**
 * @brief Initializes force-feedback effect and output state.
 *
 * Clears every slot and output gate, then activates the built-in position effect in the reserved
 * slot with centered positions, axis mode four, negative directions on both axes, and full
 * strength.
 *
 * @param[out] state Force-feedback state to initialize.
 */
void force_feedback_state_init(ForceFeedbackState *state);

/**
 * @brief Deactivates all host-controlled force-feedback effects.
 *
 * Clears the active flag in host slots zero through fifteen while preserving their configurations,
 * the built-in position effect, and both output gates.
 *
 * @param[in,out] state Force-feedback state containing host-controlled effects.
 */
void force_feedback_state_deactivate_host_effects(ForceFeedbackState *state);

/**
 * @brief Applies one decoded force-feedback command to state.
 *
 * Updates the selected effect configuration or activation state, the built-in position effect, or
 * an output gate according to the command kind. Host-effect reset and primary-output commands also
 * deactivate all host-controlled slots.
 *
 * @param[in,out] state Force-feedback state to update.
 * @param[in] command Decoded force-feedback command to apply.
 * @param[in] position_scale Positive and negative wheel-position limit for kind-2 positions.
 * @return true when the command is accepted; otherwise false.
 */
bool force_feedback_state_apply(ForceFeedbackState *state, const ForceFeedbackCommand *command,
                                int32_t position_scale);

/**
 * @brief Rescales stored kind-2 effect positions after a wheel scale change.
 *
 * Converts every kind-2 position, including inactive and built-in effects, from the previous
 * position scale to the current scale using scales shifted right by seven and integer division
 * toward zero. Equal scales return success without modifying positions; a zero shifted previous
 * scale rejects a rescale when the scales differ.
 *
 * @param[in,out] state Force-feedback state containing positions to rescale.
 * @param[in] previous_scale Position scale used for the stored positions.
 * @param[in] current_scale Replacement position scale.
 * @return true when the state is valid and rescaling succeeds; otherwise false.
 */
bool force_feedback_state_rescale_positions(ForceFeedbackState *state, int32_t previous_scale,
                                            int32_t current_scale);

#endif
