#ifndef OPENTEC_BASE_FORCE_FEEDBACK_COMMAND_H
#define OPENTEC_BASE_FORCE_FEEDBACK_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

/** @brief Force-feedback effect-slot layout used by the base controller. */
enum {
    FORCE_FEEDBACK_EFFECT_SLOT_COUNT = 16,         /**< Number of host-controlled effect slots. */
    FORCE_FEEDBACK_INTERNAL_EFFECT_SLOT_COUNT = 3, /**< Number of internal effect slots. */
    FORCE_FEEDBACK_EFFECT_RESERVED_SLOT_COUNT = 1, /**< Number of reserved effect slots. */
    FORCE_FEEDBACK_EFFECT_SLOT_CAPACITY =
        FORCE_FEEDBACK_EFFECT_SLOT_COUNT + FORCE_FEEDBACK_INTERNAL_EFFECT_SLOT_COUNT +
        FORCE_FEEDBACK_EFFECT_RESERVED_SLOT_COUNT, /**< Total effect-slot capacity. */
    FORCE_FEEDBACK_POSITION_EFFECT_SLOT =
        FORCE_FEEDBACK_EFFECT_SLOT_COUNT, /**< Built-in position-effect slot. */
    FORCE_FEEDBACK_PRIMARY_DISPLAY_EFFECT_SLOT =
        FORCE_FEEDBACK_EFFECT_SLOT_COUNT + 1, /**< Built-in primary-display effect slot. */
    FORCE_FEEDBACK_POSITION_LIMIT_EFFECT_SLOT =
        FORCE_FEEDBACK_PRIMARY_DISPLAY_EFFECT_SLOT + 1, /**< Built-in position-limit effect slot. */
};

/**
 * @brief Identifies a decoded short force-feedback command.
 *
 * The command kind selects the payload interpretation and the state transition that consumes the
 * decoded command.
 */
typedef enum {
    FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1,         /**< Configures a kind-1 effect. */
    FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2,         /**< Configures a kind-2 effect. */
    FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3,         /**< Configures a kind-3 effect. */
    FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT,             /**< Deactivates one host effect slot. */
    FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT, /**< Activates the built-in position effect. */
    FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT, /**< Deactivates the built-in position effect. */
    FORCE_FEEDBACK_COMMAND_RESET_EFFECTS,         /**< Deactivates every host effect slot. */
    FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT,    /**< Changes the primary output gate. */
    FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT,  /**< Changes the secondary output gate. */
} ForceFeedbackCommandKind;

/**
 * @brief Payload fields extracted from one supported force-feedback command.
 *
 * Common slot data is accompanied by the variant-specific effect parameters. Arrays contain the
 * two protocol axes in their wire-order positions.
 */
typedef struct {
    ForceFeedbackCommandKind kind; /**< Decoded command classification. */
    uint8_t slot; /**< Effect slot addressed by the command, including the built-in position-effect
                     slot. */
    int32_t magnitude;     /**< Signed magnitude from a kind-1 configuration. */
    uint8_t positions[2];  /**< Unsigned kind-2 positions for the two protocol axes. */
    uint8_t axis_modes[2]; /**< Kind-2 axis modes or the first kind-3 axis mode. */
    int8_t directions[2];  /**< Kind-2 or kind-3 directions for the two protocol axes. */
    uint16_t strength;     /**< Kind-2 or kind-3 effect strength. */
    uint8_t mode;          /**< Kind-3 effect mode. */
    bool output_disabled;  /**< Whether the addressed output is disabled. */
} ForceFeedbackCommand;

/**
 * @brief Decodes one supported short force-feedback output command.
 *
 * Checks for a short command with a seven-byte payload, then translates supported protocol fields
 * into the representation consumed by the force-feedback state machine.
 *
 * @param[in] output Short USB output command containing the force-feedback payload.
 * @param[out] command Destination for the decoded command fields.
 * @return true when the payload is a supported force-feedback command; otherwise false.
 */
bool force_feedback_command_decode(const UsbOutputCommand *output, ForceFeedbackCommand *command);

#endif
