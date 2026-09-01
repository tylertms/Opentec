#include "force_feedback/script_report.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Wire identifiers, offsets, and sizes used by script reports.
 *
 * The values describe the shared response envelope and each report payload's encoded fields.
 */
enum {
    SCRIPT_REPORT_ID = 0x25,                 /**< Script report identifier. */
    SCRIPT_AXES_RESPONSE_KIND = 0x2a,        /**< Axes response kind. */
    SCRIPT_AXES_RESPONSE_MODE = 8,           /**< Axes response mode. */
    SCRIPT_AXES_PADDING_SIZE = 8,            /**< Number of trailing axes-response padding bytes. */
    SCRIPT_SAMPLES_RESPONSE_KIND = 0x2b,     /**< Samples response kind. */
    SCRIPT_SAMPLES_RESPONSE_MODE = 4,        /**< Samples response mode. */
    SCRIPT_SLOT_RESPONSE_KIND = 0x23,        /**< Slot response kind. */
    SCRIPT_SLOT_RESPONSE_MODE = 5,           /**< Slot response mode. */
    SCRIPT_SLOT_AVERAGE_RATE_OFFSET = 23,    /**< Slot average-rate field offset. */
    SCRIPT_SLOT_DELTA_RATE_OFFSET = 27,      /**< Slot delta-rate field offset. */
    SCRIPT_SLOT_EXECUTION_COUNT_OFFSET = 31, /**< Slot execution-count field offset. */
    SCRIPT_SLOT_TICK_SNAPSHOT_OFFSET = 35,   /**< Slot tick-snapshot field offset. */
    SCRIPT_STATUS_RESPONSE_KIND = 0x12,      /**< Status response kind. */
    SCRIPT_STATUS_RESPONSE_MODE = 6,         /**< Status response mode. */
    SCRIPT_VALUES_RESPONSE_KIND = 0x31,      /**< Values response kind. */
    SCRIPT_VALUES_RESPONSE_MODE = 7,         /**< Values response mode. */
    SCRIPT_RESPONSE_PAYLOAD_OFFSET = 5,      /**< First response payload byte offset. */
    SCRIPT_STATUS_RUNTIME_MODE_OFFSET = 21,  /**< Status runtime-mode field offset. */
    SCRIPT_TIMING_VARIABLE_FIRST = 8,        /**< First timing variable index. */
    SCRIPT_TIMING_VARIABLE_COUNT = 4,        /**< Number of timing variables reported. */
};

/**
 * @brief Encodes a transport-neutral script response envelope.
 *
 * Writes report ID 0x25, a zeroed reserved byte, a zeroed sequence placeholder, response kind, and
 * response mode shared by script query replies. The active transport fills the sequence placeholder
 * before transmission.
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
