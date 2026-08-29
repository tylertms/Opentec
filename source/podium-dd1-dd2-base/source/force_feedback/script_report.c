#include "force_feedback/script_report.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SCRIPT_REPORT_ID = 0x25,
    SCRIPT_AXES_RESPONSE_KIND = 0x2a,
    SCRIPT_AXES_RESPONSE_MODE = 8,
    SCRIPT_AXES_PADDING_SIZE = 8,
    SCRIPT_SAMPLES_RESPONSE_KIND = 0x2b,
    SCRIPT_SAMPLES_RESPONSE_MODE = 4,
    SCRIPT_SLOT_RESPONSE_KIND = 0x23,
    SCRIPT_SLOT_RESPONSE_MODE = 5,
    SCRIPT_SLOT_AVERAGE_RATE_OFFSET = 23,
    SCRIPT_SLOT_DELTA_RATE_OFFSET = 27,
    SCRIPT_SLOT_EXECUTION_COUNT_OFFSET = 31,
    SCRIPT_SLOT_TICK_SNAPSHOT_OFFSET = 35,
    SCRIPT_STATUS_RESPONSE_KIND = 0x12,
    SCRIPT_STATUS_RESPONSE_MODE = 6,
    SCRIPT_VALUES_RESPONSE_KIND = 0x31,
    SCRIPT_VALUES_RESPONSE_MODE = 7,
    SCRIPT_RESPONSE_PAYLOAD_OFFSET = 5,
    SCRIPT_STATUS_RUNTIME_MODE_OFFSET = 21,
    SCRIPT_TIMING_VARIABLE_FIRST = 8,
    SCRIPT_TIMING_VARIABLE_COUNT = 4,
};

/**
 * @brief Encodes a transport-neutral script response envelope.
 *
 * Writes the report ID, reserved bytes, response kind, and response mode shared by script query
 * replies. The active transport assigns the response sequence before transmission.
 *
 * @param[in] kind Response payload kind.
 * @param[in] mode Response query mode.
 * @param[out] response Destination for the five-byte envelope.
 */
static void encode_header(uint8_t kind, uint8_t mode, uint8_t *response) {
    response[0] = SCRIPT_REPORT_ID;
    response[1] = 0;
    response[2] = 0;
    response[3] = kind;
    response[4] = mode;
}

/**
 * @brief Takes the next script-report response sequence.
 *
 * Returns the current sequence and advances it. Value 255 produces sequence 1 and leaves 1 as the
 * next state.
 *
 * @param[in,out] next_sequence Sequence state to consume and advance.
 * @return Sequence value for the current response.
 */
uint8_t force_feedback_script_report_sequence_take(uint8_t *next_sequence) {
    uint8_t sequence = *next_sequence;
    if (sequence == UINT8_MAX) {
        *next_sequence = 1;
        return 1;
    }

    *next_sequence = sequence + 1u;
    return sequence;
}

/**
 * @brief Encodes one 32-bit script value in least-significant-byte-first order.
 *
 * Writes all four bytes of the value in the order used by the script query reports.
 *
 * @param[out] output Destination for four encoded bytes.
 * @param[in] value Script value to encode.
 */
static void encode_value(uint8_t output[4], uint32_t value) {
    for (uint8_t index = 0; index < 4; index++) {
        output[index] = (uint8_t)(value >> (index * 8u));
    }
}

/**
 * @brief Encodes the script axis vendor response.
 *
 * Writes the 25 00 SS 2A 08 envelope, axis group zero, eight 32-bit axis values, and eight zero
 * padding bytes.
 *
 * @param[in] runtime Current force-feedback script runtime.
 * @param[out] response Destination for the complete 46-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the complete response was encoded.
 */
bool force_feedback_script_axes_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t *response, size_t length) {
    if (runtime == NULL || response == NULL || length < FORCE_FEEDBACK_SCRIPT_AXES_RESPONSE_SIZE) {
        return false;
    }

    encode_header(SCRIPT_AXES_RESPONSE_KIND, SCRIPT_AXES_RESPONSE_MODE, response);
    response[SCRIPT_RESPONSE_PAYLOAD_OFFSET] = 0;
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_AXIS_REPORT_COUNT; index++) {
        encode_value(response + SCRIPT_RESPONSE_PAYLOAD_OFFSET + 1u + index * 4u,
                     runtime->axes[index]);
    }
    for (uint8_t index = 0; index < SCRIPT_AXES_PADDING_SIZE; index++) {
        response[SCRIPT_RESPONSE_PAYLOAD_OFFSET + 1u +
                 FORCE_FEEDBACK_SCRIPT_AXIS_REPORT_COUNT * 4u + index] = 0;
    }
    return true;
}

/**
 * @brief Encodes a ten-value script sample vendor response.
 *
 * Writes the 25 00 SS 2B 04 envelope followed by ten consecutive sample values beginning at the
 * requested index in least-significant-byte-first order.
 *
 * @param[in] runtime Current force-feedback script runtime.
 * @param[in] first_sample Index of the first sample from zero through 501.
 * @param[out] response Destination for the complete 47-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the query is in range and the complete response was encoded.
 */
bool force_feedback_script_samples_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                 uint16_t first_sample, uint8_t *response,
                                                 size_t length) {
    if (runtime == NULL || first_sample > FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_LAST_FIRST ||
        response == NULL || length < FORCE_FEEDBACK_SCRIPT_SAMPLES_RESPONSE_SIZE) {
        return false;
    }

    encode_header(SCRIPT_SAMPLES_RESPONSE_KIND, SCRIPT_SAMPLES_RESPONSE_MODE, response);
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_COUNT; index++) {
        encode_value(response + SCRIPT_RESPONSE_PAYLOAD_OFFSET + index * 4u,
                     runtime->samples.values[first_sample + index]);
    }
    return true;
}

/**
 * @brief Encodes one script slot's detail vendor response.
 *
 * Writes the 25 00 SS 23 05 envelope, slot index, raw slot state, four slot values, average and
 * delta rates, execution count, and tick snapshot in least-significant-byte-first order. Query
 * index 15 produces the accepted empty response with a zeroed payload.
 *
 * @param[in] runtime Current force-feedback script runtime.
 * @param[in] slot Reportable script slot index from zero through 14, or empty query index 15.
 * @param[out] response Destination for the complete 39-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the slot query is accepted and the complete response was encoded.
 */
bool force_feedback_script_slot_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t slot, uint8_t *response, size_t length) {
    if (runtime == NULL || slot > FORCE_FEEDBACK_SCRIPT_SLOT_REPORT_EMPTY || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_SLOT_RESPONSE_SIZE) {
        return false;
    }

    encode_header(SCRIPT_SLOT_RESPONSE_KIND, SCRIPT_SLOT_RESPONSE_MODE, response);
    if (slot == FORCE_FEEDBACK_SCRIPT_SLOT_REPORT_EMPTY) {
        for (uint8_t index = SCRIPT_RESPONSE_PAYLOAD_OFFSET;
             index < FORCE_FEEDBACK_SCRIPT_SLOT_RESPONSE_SIZE; index++) {
            response[index] = 0;
        }
        return true;
    }

    const ForceFeedbackScriptSlot *selected = &runtime->slots[slot];
    response[SCRIPT_RESPONSE_PAYLOAD_OFFSET] = slot;
    response[SCRIPT_RESPONSE_PAYLOAD_OFFSET + 1u] = selected->state;
    for (uint8_t index = 0; index < 4; index++) {
        encode_value(response + SCRIPT_RESPONSE_PAYLOAD_OFFSET + 2u + index * 4u,
                     selected->values[index]);
    }
    encode_value(response + SCRIPT_SLOT_AVERAGE_RATE_OFFSET, selected->average_rate);
    encode_value(response + SCRIPT_SLOT_DELTA_RATE_OFFSET, selected->delta_rate);
    encode_value(response + SCRIPT_SLOT_EXECUTION_COUNT_OFFSET, selected->execution_count);
    encode_value(response + SCRIPT_SLOT_TICK_SNAPSHOT_OFFSET, selected->tick_snapshot);
    return true;
}

/**
 * @brief Encodes the script-slot status vendor response.
 *
 * Writes the 25 00 SS 12 06 envelope, the 16 slot states, and the current runtime mode. An
 * internal fault state is serialized as state 4.
 *
 * @param[in] runtime Current force-feedback script runtime.
 * @param[in] mode Current force-feedback runtime mode.
 * @param[out] response Destination for the complete 22-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the complete response was encoded.
 */
bool force_feedback_script_status_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                ForceFeedbackRuntimeMode mode, uint8_t *response,
                                                size_t length) {
    if (runtime == NULL || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE) {
        return false;
    }

    encode_header(SCRIPT_STATUS_RESPONSE_KIND, SCRIPT_STATUS_RESPONSE_MODE, response);
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        ForceFeedbackScriptSlotState state = runtime->slots[slot].state;
        response[SCRIPT_RESPONSE_PAYLOAD_OFFSET + slot] =
            state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT ? FORCE_FEEDBACK_SCRIPT_SLOT_SERIALIZED_FAULT
                                                      : state;
    }
    response[SCRIPT_STATUS_RUNTIME_MODE_OFFSET] = mode;
    return true;
}

/**
 * @brief Encodes the script timing and variable vendor response.
 *
 * Writes the 25 00 SS 31 07 envelope, followed by the four read-only engine timing values and the
 * eight writable script variables in least-significant-byte-first order.
 *
 * @param[in] runtime Current force-feedback script runtime.
 * @param[out] response Destination for the complete 53-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the complete response was encoded.
 */
bool force_feedback_script_values_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                uint8_t *response, size_t length) {
    if (runtime == NULL || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE) {
        return false;
    }

    encode_header(SCRIPT_VALUES_RESPONSE_KIND, SCRIPT_VALUES_RESPONSE_MODE, response);
    for (uint8_t index = 0; index < SCRIPT_TIMING_VARIABLE_COUNT; index++) {
        encode_value(response + SCRIPT_RESPONSE_PAYLOAD_OFFSET + index * 4u,
                     runtime->variables[SCRIPT_TIMING_VARIABLE_FIRST + index]);
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_WRITABLE_VARIABLE_COUNT; index++) {
        encode_value(response + SCRIPT_RESPONSE_PAYLOAD_OFFSET + SCRIPT_TIMING_VARIABLE_COUNT * 4u +
                         index * 4u,
                     runtime->variables[index]);
    }
    return true;
}
