#include "force_feedback/script_runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Host packet identifiers and field offsets used by the script runtime.
 *
 * The values identify supported packet kinds and the input fields used to update runtime state.
 */
enum {
    SCRIPT_SAMPLE_PACKET = 0x0b,        /**< Script sample-update packet opcode. */
    SCRIPT_CONTROL_PACKET = 0x0c,       /**< Script slot-control packet opcode. */
    SCRIPT_UPLOAD_PACKET = 0x0d,        /**< Script upload packet opcode. */
    SCRIPT_INPUT_PACKET = 0x0e,         /**< Script input packet opcode. */
    SCRIPT_INPUT_STATUS_OFFSET = 4,     /**< Input packet status byte offset. */
    SCRIPT_INPUT_FIRST_SLOT_OFFSET = 9, /**< First input-slot field offset. */
    SCRIPT_INPUT_SLOT_SIZE = 9,         /**< Input-slot field size in bytes. */
    SCRIPT_MOTION_POSITION = 4,         /**< Runtime motion index containing normalized position. */
};

/**
 * @brief Decodes one little-endian 32-bit script value.
 *
 * Combines four consecutive packet bytes without requiring aligned storage.
 *
 * @param[in] data Four-byte little-endian source.
 * @return Decoded unsigned value.
 */
static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

void force_feedback_script_runtime_init(ForceFeedbackScriptSystem *system) {
    if (system == NULL) {
        return;
    }

    system->values = (ForceFeedbackScriptRuntime){0};
    force_feedback_script_samples_init(&system->values.samples);
    force_feedback_script_store_init(&system->store);
    force_feedback_script_inputs_init(&system->inputs);
    system->clock = (ForceFeedbackScriptClock){0};
    system->motion = (ForceFeedbackScriptMotionState){0};
    system->scheduler = (ForceFeedbackScriptScheduler){0};
    system->host_tick_snapshot = 0;
    system->idle_tick_snapshot = 0;
    system->mode = FORCE_FEEDBACK_RUNTIME_POSITION_ONLY;
    system->store.position_request_pending = true;
}

bool force_feedback_script_runtime_apply_packet(ForceFeedbackScriptSystem *system,
                                                const uint8_t *packet, size_t length) {
    if (system == NULL || packet == NULL || length == 0) {
        return false;
    }

    switch (packet[0]) {
    case SCRIPT_SAMPLE_PACKET:
        return force_feedback_script_samples_apply(&system->values.samples, packet, length);
    case SCRIPT_CONTROL_PACKET:
        return force_feedback_script_runtime_apply_control(system, packet, length);
    case SCRIPT_UPLOAD_PACKET:
        return force_feedback_script_store_upload(&system->store, system->values.slots, packet,
                                                  length);
    case SCRIPT_INPUT_PACKET:
        if (!force_feedback_script_inputs_apply(
                &system->inputs,
                system->values.variables[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT_VARIABLE], packet,
                length)) {
            return false;
        }
        if (packet[SCRIPT_INPUT_STATUS_OFFSET] == FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE ||
            packet[SCRIPT_INPUT_STATUS_OFFSET] == FORCE_FEEDBACK_SCRIPT_INPUT_READY) {
            for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT; index++) {
                size_t offset = SCRIPT_INPUT_FIRST_SLOT_OFFSET + index * SCRIPT_INPUT_SLOT_SIZE;
                if (packet[offset] == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION) {
                    system->values.motion[SCRIPT_MOTION_POSITION] = read_u32(&packet[offset + 1]);
                }
            }
        }
        return true;
    default:
        return false;
    }
}

bool force_feedback_script_runtime_apply_control(ForceFeedbackScriptSystem *system,
                                                 const uint8_t *packet, size_t length) {
    if (system == NULL) {
        return false;
    }

    ForceFeedbackScriptControlResult result = force_feedback_script_control_decode(packet, length);
    if (!result.valid) {
        return false;
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; index++) {
        force_feedback_script_slot_apply(&system->values.slots[index], result.value.slots[index]);
    }
    system->mode = result.value.runtime_mode;
    force_feedback_script_store_compact(&system->store, system->values.slots);
    return true;
}
