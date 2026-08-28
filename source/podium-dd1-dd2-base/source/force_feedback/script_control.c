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
 * @brief Decode the script-slot controls and force-feedback runtime mode.
 *
 * Reads two four-bit slot commands from each of bytes 4 through 11 and reads the shared runtime
 * mode from byte 12 of a script-control feature packet.
 *
 * @param[in] packet Feature-command packet beginning with the script-control opcode.
 * @param[in] length Number of available packet bytes.
 * @return The decoded controls and whether the packet contains the complete command.
 */
ForceFeedbackScriptControlResult force_feedback_script_control_decode(const uint8_t *packet,
                                                                      size_t length) {
    ForceFeedbackScriptControlResult result = {0};
    if (packet == NULL || length < SCRIPT_CONTROL_PACKET_SIZE ||
        packet[0] != SCRIPT_CONTROL_OPCODE) {
        return result;
    }

    for (uint8_t index = 0; index < SCRIPT_CONTROL_PACKED_SLOT_COUNT; index++) {
        uint8_t packed = packet[SCRIPT_CONTROL_FIRST_SLOT_OFFSET + index];
        result.value.slots[index * 2] = packed >> 4;
        result.value.slots[index * 2 + 1] = packed & 0x0f;
    }
    result.value.runtime_mode = packet[SCRIPT_CONTROL_RUNTIME_MODE_OFFSET];
    result.valid = true;
    return result;
}

/**
 * @brief Encode script-slot states and the force-feedback runtime mode.
 *
 * Writes the 16 slot states at response bytes 5 through 20 and the runtime mode at byte 21. An
 * internal fault state is serialized as state 4.
 *
 * @param[in] status Current per-slot states and shared runtime mode.
 * @param[out] response Destination feature response whose five-byte prefix is already prepared.
 * @param[in] length Number of writable response bytes.
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
 * @brief Apply a lifecycle command to one force-feedback script slot.
 *
 * Clear always empties the slot. Start is ignored for an empty slot; otherwise it clears all four
 * slot values and all four timing metrics before activating the slot. Stop is ignored for an empty
 * slot. Pause and resume apply only to active and paused slots, respectively.
 *
 * @param[in,out] slot Script slot to update.
 * @param[in] command Requested clear, start, stop, pause, or resume operation.
 * @return True when the command changes or clears the slot state.
 */
bool force_feedback_script_slot_apply(ForceFeedbackScriptSlot *slot,
                                      ForceFeedbackScriptSlotCommand command) {
    if (slot == NULL) {
        return false;
    }

    switch (command) {
    case FORCE_FEEDBACK_SCRIPT_SLOT_CLEAR:
        slot->state = FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_START:
        if (slot->state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY) {
            return false;
        }
        for (uint8_t index = 0; index < 4; ++index) {
            slot->values[index] = 0;
        }
        slot->average_rate = 0;
        slot->delta_rate = 0;
        slot->execution_count = 0;
        slot->tick_snapshot = 0;
        slot->state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_STOP:
        if (slot->state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY) {
            return false;
        }
        slot->state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE:
        if (slot->state != FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE) {
            return false;
        }
        slot->state = FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED;
        return true;
    case FORCE_FEEDBACK_SCRIPT_SLOT_RESUME:
        if (slot->state != FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED) {
            return false;
        }
        slot->state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
        return true;
    default:
        return false;
    }
}

/**
 * @brief Advance the force-feedback timer state by one interrupt.
 *
 * Advances the engine clock in active and zero-output modes, advances the active slot clock while
 * a script is executing, and always advances the motion clock.
 *
 * @param[in,out] clock Runtime clock state containing engine, slot, and motion counters.
 * @param[in] mode Current force-feedback runtime mode.
 */
void force_feedback_script_clock_tick(ForceFeedbackScriptClock *clock,
                                      ForceFeedbackRuntimeMode mode) {
    if (clock == NULL) {
        return;
    }

    if (mode == FORCE_FEEDBACK_RUNTIME_ACTIVE || mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT) {
        clock->ticks++;
    }
    if (clock->script_executing && clock->active_slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
        clock->slot_ticks[clock->active_slot]++;
    }
    clock->motion_ticks++;
}
