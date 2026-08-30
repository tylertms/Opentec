#include "force_feedback/script_input.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SAMPLE_PACKET_OPCODE = 0x0b,
    SAMPLE_FIRST_OFFSET = 4,
    SAMPLE_RECORD_SIZE = 6,
    SAMPLE_UNUSED = UINT16_MAX,
    INPUT_PACKET_OPCODE = 0x0e,
    INPUT_STATUS_OFFSET = 4,
    INPUT_DEADLINE_OFFSET = 5,
    INPUT_SAMPLE_COUNT_OFFSET = 7,
    INPUT_FIRST_SLOT_OFFSET = 9,
    INPUT_SLOT_SIZE = 9,
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

/**
 * @brief Initialize the script sample table.
 *
 * Marks every one of the 512 script sample values as unused with an all-ones 32-bit value.
 *
 * @param[out] samples Script sample table to initialize.
 */
void force_feedback_script_samples_init(ForceFeedbackScriptSamples *samples) {
    if (samples == NULL) {
        return;
    }
    for (uint16_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT; index++) {
        samples->values[index] = UINT32_MAX;
    }
}

/**
 * @brief Apply a script sample-point packet atomically.
 *
 * Validates all ten little-endian sample indexes before writing any values. Indexes from 0 through
 * 511 select table entries, while index 65535 skips its record. Any other index rejects the entire
 * packet without changing the sample table.
 *
 * @param[in,out] samples Script sample table to update.
 * @param[in] packet Complete 64-byte vendor-HID packet beginning with opcode 0x0B.
 * @param[in] length Number of available packet bytes.
 * @return True when every sample index is valid and all selected values are written.
 */
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

/**
 * @brief Initialize the live force-feedback script inputs.
 *
 * Selects position input status, clears the deadline, sample count, shared position, values, and
 * durations, and marks each of the three input slots unused.
 *
 * @param[out] inputs Live script-input state to initialize.
 */
void force_feedback_script_inputs_init(ForceFeedbackScriptInputs *inputs) {
    if (inputs == NULL) {
        return;
    }
    *inputs = (ForceFeedbackScriptInputs){0};
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT; slot++) {
        inputs->slots[slot].status = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED;
    }
}

/**
 * @brief Apply live script inputs from a vendor-HID packet.
 *
 * Always stores the packet status. Status 1 or 240 also updates the deadline, sample count, and
 * each slot whose status is not 255. A slot with status 0 additionally replaces the shared position
 * value. Other packet statuses leave all fields except the packet status unchanged.
 *
 * @param[in,out] inputs Live script-input state to update.
 * @param[in] current_sample_count Current script-engine sample counter used as the deadline base.
 * @param[in] packet Complete 64-byte vendor-HID packet beginning with opcode 0x0E.
 * @param[in] length Number of available packet bytes.
 * @return True when the packet has the live-input opcode and complete vendor-HID length.
 */
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
