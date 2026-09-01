#include "force_feedback/script_input.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encoded offsets and sizes for force-feedback script input packets.
 *
 * The constants describe the fixed vendor-HID layouts consumed by this module.
 */
enum {
    SAMPLE_PACKET_OPCODE = 0x0b,   /**< Sample-table packet opcode. */
    SAMPLE_FIRST_OFFSET = 4,       /**< Offset of the first sample record. */
    SAMPLE_RECORD_SIZE = 6,        /**< Size of one sample index/value record. */
    SAMPLE_UNUSED = UINT16_MAX,    /**< Sample index that leaves an entry unchanged. */
    INPUT_PACKET_OPCODE = 0x0e,    /**< Live-input packet opcode. */
    INPUT_STATUS_OFFSET = 4,       /**< Offset of the live-input status byte. */
    INPUT_DEADLINE_OFFSET = 5,     /**< Offset of the two-byte deadline delta. */
    INPUT_SAMPLE_COUNT_OFFSET = 7, /**< Offset of the two-byte sample count. */
    INPUT_FIRST_SLOT_OFFSET = 9,   /**< Offset of the first live-input slot. */
    INPUT_SLOT_SIZE = 9,           /**< Size of one live-input slot record. */
};

/**
 * @brief Reads a little-endian 16-bit packet field.
 *
 * Combines two consecutive bytes with the least significant byte first.
 *
 * @param[in] data Two-byte field.
 * @return The decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

/**
 * @brief Reads a little-endian 32-bit packet field.
 *
 * Combines four consecutive bytes with the least significant byte first.
 *
 * @param[in] data Four-byte field.
 * @return The decoded unsigned value.
 */
static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

void force_feedback_script_samples_init(ForceFeedbackScriptSamples *samples) {
    if (samples == NULL) {
        return;
    }
    for (uint16_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT; index++) {
        samples->values[index] = UINT32_MAX;
    }
}

bool force_feedback_script_samples_apply(ForceFeedbackScriptSamples *samples, const uint8_t *packet,
                                         size_t length) {
    if (samples == NULL || packet == NULL || length != FORCE_FEEDBACK_SCRIPT_PACKET_SIZE ||
        packet[0] != SAMPLE_PACKET_OPCODE) {
        return false;
    }

    for (uint8_t record = 0; record < FORCE_FEEDBACK_SCRIPT_SAMPLE_UPDATE_COUNT; record++) {
        size_t offset = SAMPLE_FIRST_OFFSET + (size_t)record * SAMPLE_RECORD_SIZE;
        uint16_t index = read_u16(&packet[offset]);
        if (index >= FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT && index != SAMPLE_UNUSED) {
            return false;
        }
    }

    for (uint8_t record = 0; record < FORCE_FEEDBACK_SCRIPT_SAMPLE_UPDATE_COUNT; record++) {
        size_t offset = SAMPLE_FIRST_OFFSET + (size_t)record * SAMPLE_RECORD_SIZE;
        uint16_t index = read_u16(&packet[offset]);
        if (index != SAMPLE_UNUSED) {
            samples->values[index] = read_u32(&packet[offset + 2]);
        }
    }
    return true;
}

void force_feedback_script_inputs_init(ForceFeedbackScriptInputs *inputs) {
    if (inputs == NULL) {
        return;
    }
    *inputs = (ForceFeedbackScriptInputs){0};
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT; slot++) {
        inputs->slots[slot].status = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED;
    }
}

bool force_feedback_script_inputs_apply(ForceFeedbackScriptInputs *inputs,
                                        uint32_t current_sample_count, const uint8_t *packet,
                                        size_t length) {
    if (inputs == NULL || packet == NULL || length != FORCE_FEEDBACK_SCRIPT_PACKET_SIZE ||
        packet[0] != INPUT_PACKET_OPCODE) {
        return false;
    }

    inputs->status = packet[INPUT_STATUS_OFFSET];
    if (inputs->status != FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE &&
        inputs->status != FORCE_FEEDBACK_SCRIPT_INPUT_READY) {
        return true;
    }

    inputs->deadline = current_sample_count + read_u16(&packet[INPUT_DEADLINE_OFFSET]);
    inputs->sample_count = read_u16(&packet[INPUT_SAMPLE_COUNT_OFFSET]);
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT; slot++) {
        size_t offset = INPUT_FIRST_SLOT_OFFSET + (size_t)slot * INPUT_SLOT_SIZE;
        ForceFeedbackScriptInputStatus status = packet[offset];
        if (status == FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED) {
            continue;
        }
        inputs->slots[slot] = (ForceFeedbackScriptInputSlot){
            .status = status,
            .value = read_u32(&packet[offset + 1]),
            .duration = read_u32(&packet[offset + 5]),
        };
        if (status == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION) {
            inputs->position_value = inputs->slots[slot].value;
        }
    }
    return true;
}
