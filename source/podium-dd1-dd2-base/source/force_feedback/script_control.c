#include "force_feedback/script_control.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encoded offsets and sizes for force-feedback script control packets.
 */
enum {
    SCRIPT_CONTROL_OPCODE = 0x0c,            /**< Script-control packet opcode. */
    SCRIPT_CONTROL_FIRST_SLOT_OFFSET = 4,    /**< Offset of the first packed slot-command byte. */
    SCRIPT_CONTROL_PACKED_SLOT_COUNT = 8,    /**< Number of packed bytes carrying slot commands. */
    SCRIPT_CONTROL_RUNTIME_MODE_OFFSET = 12, /**< Offset of the runtime-mode byte. */
    SCRIPT_CONTROL_PACKET_SIZE = 13,         /**< Minimum packet length required for decoding. */
};

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
