#include "force_feedback/script_control.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SCRIPT_CONTROL_OPCODE = 0x0c,
    SCRIPT_CONTROL_FIRST_SLOT_OFFSET = 4,
    SCRIPT_CONTROL_PACKED_SLOT_COUNT = 8,
    SCRIPT_CONTROL_RUNTIME_MODE_OFFSET = 12,
    SCRIPT_CONTROL_PACKET_SIZE = 13,
    SCRIPT_STATUS_FIRST_SLOT_OFFSET = 5,
    SCRIPT_STATUS_RUNTIME_MODE_OFFSET = 21,
    SCRIPT_STATUS_RESPONSE_SIZE = 22,
};

/**
 * Decodes all 16 packed force-feedback script slot commands and the shared runtime mode.
 *
 * @param packet Feature-command packet beginning with the script-control opcode.
 * @param length Number of available packet bytes.
 * @param control Destination for the per-slot commands and runtime mode.
 * @return True when the packet contains the complete script-control command.
 */
bool force_feedback_script_control_decode(const uint8_t *packet, size_t length,
                                          ForceFeedbackScriptControl *control) {
    if (packet == NULL || control == NULL || length < SCRIPT_CONTROL_PACKET_SIZE ||
        packet[0] != SCRIPT_CONTROL_OPCODE) {
        return false;
    }

    for (uint8_t index = 0; index < SCRIPT_CONTROL_PACKED_SLOT_COUNT; index++) {
        uint8_t packed = packet[SCRIPT_CONTROL_FIRST_SLOT_OFFSET + index];
        control->slots[index * 2] = packed >> 4;
        control->slots[index * 2 + 1] = packed & 0x0f;
    }
    control->runtime_mode = packet[SCRIPT_CONTROL_RUNTIME_MODE_OFFSET];
    return true;
}

/**
 * Writes the 16 script slot states and runtime mode into a feature response.
 *
 * @param status Current per-slot states and shared runtime mode.
 * @param response Destination feature response whose five-byte prefix is already prepared.
 * @param length Number of writable response bytes.
 * @return True when the complete status fields fit in the response.
 */
bool force_feedback_script_status_encode(const ForceFeedbackScriptStatus *status, uint8_t *response,
                                         size_t length) {
    if (status == NULL || response == NULL || length < SCRIPT_STATUS_RESPONSE_SIZE) {
        return false;
    }

    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        ForceFeedbackScriptSlotState state = status->slots[slot];
        response[SCRIPT_STATUS_FIRST_SLOT_OFFSET + slot] =
            state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT ? FORCE_FEEDBACK_SCRIPT_SLOT_SERIALIZED_FAULT
                                                      : state;
    }
    response[SCRIPT_STATUS_RUNTIME_MODE_OFFSET] = status->runtime_mode;
    return true;
}

/**
 * Applies one force-feedback script slot command to its logical runtime state.
 *
 * @param current Current script slot state.
 * @param command Requested clear, start, stop, pause, or resume operation.
 * @param transition Destination for the resulting state and runtime-reset request.
 * @return True when the command is accepted in the current state.
 */
bool force_feedback_script_slot_transition(ForceFeedbackScriptSlotState current,
                                           ForceFeedbackScriptSlotCommand command,
                                           ForceFeedbackScriptSlotTransition *transition) {
    if (transition == NULL) {
        return false;
    }

    *transition = (ForceFeedbackScriptSlotTransition){
        .state = current,
        .reset_runtime = false,
    };

    switch (command) {
    case FORCE_FEEDBACK_SCRIPT_SLOT_CLEAR:
        transition->state = FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_START:
        if (current == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY) {
            return false;
        }
        transition->state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
        transition->reset_runtime = true;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_STOP:
        if (current == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY) {
            return false;
        }
        transition->state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE:
        if (current != FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE) {
            return false;
        }
        transition->state = FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_RESUME:
        if (current != FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED) {
            return false;
        }
        transition->state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
        return true;
    default:
        return false;
    }
}
