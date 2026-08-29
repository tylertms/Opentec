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
 * @brief Encodes a script response envelope and advances its sequence.
 *
 * Advances the sequence with a nonzero wrap and writes the report ID, reserved byte, sequence,
 * response kind, and response mode shared by script query replies.
 *
 * @param[in,out] sequence Shared nonzero vendor response sequence.
 * @param[in] kind Response payload kind.
 * @param[in] mode Response query mode.
 * @param[out] response Destination for the five-byte envelope.
 */
static void encode_header(uint8_t *sequence, uint8_t kind, uint8_t mode, uint8_t *response) {
    *sequence = *sequence == UINT8_MAX ? 1 : (uint8_t)(*sequence + 1u);
    response[0] = SCRIPT_REPORT_ID;
    response[1] = 0;
    response[2] = *sequence;
    response[3] = kind;
    response[4] = mode;
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
 * @param[in,out] sequence Shared nonzero vendor response sequence.
 * @param[out] response Destination for the complete 46-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the complete response was encoded.
 */
bool force_feedback_script_axes_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t *sequence, uint8_t *response, size_t length) {
    if (runtime == NULL || sequence == NULL || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_AXES_RESPONSE_SIZE) {
        return false;
    }

    encode_header(sequence, SCRIPT_AXES_RESPONSE_KIND, SCRIPT_AXES_RESPONSE_MODE, response);
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
 * @param[in,out] sequence Shared nonzero vendor response sequence.
 * @param[out] response Destination for the complete 47-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the query is in range and the complete response was encoded.
 */
bool force_feedback_script_samples_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                 uint16_t first_sample, uint8_t *sequence,
                                                 uint8_t *response, size_t length) {
    if (runtime == NULL || first_sample > FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_LAST_FIRST ||
        sequence == NULL || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_SAMPLES_RESPONSE_SIZE) {
        return false;
    }

    encode_header(sequence, SCRIPT_SAMPLES_RESPONSE_KIND, SCRIPT_SAMPLES_RESPONSE_MODE, response);
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_COUNT; index++) {
        encode_value(response + SCRIPT_RESPONSE_PAYLOAD_OFFSET + index * 4u,
                     runtime->samples.values[first_sample + index]);
    }
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
 * @param[in,out] sequence Shared nonzero vendor response sequence.
 * @param[out] response Destination for the complete 22-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the complete response was encoded.
 */
bool force_feedback_script_status_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                ForceFeedbackRuntimeMode mode, uint8_t *sequence,
                                                uint8_t *response, size_t length) {
    if (runtime == NULL || sequence == NULL || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE) {
        return false;
    }

    encode_header(sequence, SCRIPT_STATUS_RESPONSE_KIND, SCRIPT_STATUS_RESPONSE_MODE, response);
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
 * @param[in,out] sequence Shared nonzero vendor response sequence.
 * @param[out] response Destination for the complete 53-byte response.
 * @param[in] length Number of writable response bytes.
 * @return True when the complete response was encoded.
 */
bool force_feedback_script_values_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                uint8_t *sequence, uint8_t *response,
                                                size_t length) {
    if (runtime == NULL || sequence == NULL || response == NULL ||
        length < FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE) {
        return false;
    }

    encode_header(sequence, SCRIPT_VALUES_RESPONSE_KIND, SCRIPT_VALUES_RESPONSE_MODE, response);
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
