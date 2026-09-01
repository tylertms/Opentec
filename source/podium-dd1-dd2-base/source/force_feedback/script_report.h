#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_REPORT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

/**
 * @brief Sizes and bounds for script query responses.
 *
 * The constants describe response payload lengths and the valid starting indexes for report
 * queries.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_AXIS_REPORT_COUNT = 8, /**< Number of axis values in an axes response. */
    FORCE_FEEDBACK_SCRIPT_AXES_RESPONSE_SIZE = 46, /**< Required bytes for an axes response. */
    FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_COUNT =
        10, /**< Number of sample values in a samples response. */
    FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_LAST_FIRST = 501, /**< Last valid first sample index. */
    FORCE_FEEDBACK_SCRIPT_SAMPLES_RESPONSE_SIZE = 47, /**< Required bytes for a samples response. */
    FORCE_FEEDBACK_SCRIPT_SLOT_REPORT_LAST = 14,      /**< Last reportable script slot index. */
    FORCE_FEEDBACK_SCRIPT_SLOT_REPORT_EMPTY = 15,    /**< Query index for an empty slot response. */
    FORCE_FEEDBACK_SCRIPT_SLOT_RESPONSE_SIZE = 39,   /**< Required bytes for a slot response. */
    FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE = 22, /**< Required bytes for a status response. */
    FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE = 53, /**< Required bytes for a values response. */
};

/**
 * @brief Take and advance a script-report response sequence number.
 *
 * Returns the current sequence and advances the supplied state. When the current value is 255, the
 * returned value and next state are both 1.
 *
 * @param[in,out] next_sequence Sequence state to consume and advance.
 * @return The sequence value assigned to the current response.
 * @pre next_sequence points to a valid sequence state.
 */
uint8_t force_feedback_script_report_sequence_take(uint8_t *next_sequence);

/**
 * @brief Encode the script axes query response.
 *
 * Writes the 25 00 00 2A 08 response prefix, axis group zero, eight raw axis values in
 * least-significant-byte-first order, and eight trailing padding bytes into the supplied buffer.
 * The transport fills the sequence byte at index 2 before transmission.
 *
 * @param[in] runtime Current script runtime containing axis values.
 * @param[out] response Destination response buffer.
 * @param[in] length Number of writable bytes in response.
 * @return true when runtime and response are non-null and length is at least 46; otherwise false.
 */
bool force_feedback_script_axes_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t *response, size_t length);

/**
 * @brief Encode a script samples query response.
 *
 * Writes the 25 00 00 2B 04 response prefix followed by ten consecutive raw sample values
 * beginning at first_sample in least-significant-byte-first order into the supplied buffer. The
 * encoder fills bytes 0 through 44; the final two bytes of the 47-byte response remain unchanged.
 * The transport fills the sequence byte at index 2 before transmission.
 *
 * @param[in] runtime Current script runtime containing sample values.
 * @param[in] first_sample Index of the first sample value.
 * @param[out] response Destination response buffer.
 * @param[in] length Number of writable bytes in response.
 * @return true when runtime and response are non-null, first_sample is at most 501, and length is
 * at least 47; otherwise false.
 */
bool force_feedback_script_samples_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                 uint16_t first_sample, uint8_t *response,
                                                 size_t length);

/**
 * @brief Encode one script slot query response.
 *
 * Writes the 25 00 00 23 05 response prefix, one slot's state, values, and timing metrics in
 * least-significant-byte-first order, or a zeroed payload for the empty-slot query index. The
 * transport fills the sequence byte at index 2 before transmission.
 *
 * @param[in] runtime Current script runtime containing slot state.
 * @param[in] slot Slot index from zero through 14, or the empty query index 15.
 * @param[out] response Destination response buffer.
 * @param[in] length Number of writable bytes in response.
 * @return true when runtime and response are non-null, slot is at most 15, and length is at least
 * 39; otherwise false.
 */
bool force_feedback_script_slot_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t slot, uint8_t *response, size_t length);

/**
 * @brief Encode the script slot-status query response.
 *
 * Writes the 25 00 00 12 06 response prefix, all slot states, and the supplied runtime mode,
 * translating an internal fault state to its serialized fault value. The transport fills the
 * sequence byte at index 2 before transmission.
 *
 * @param[in] runtime Current script runtime containing slot states.
 * @param[in] mode Runtime mode to encode.
 * @param[out] response Destination response buffer.
 * @param[in] length Number of writable bytes in response.
 * @return true when runtime and response are non-null and length is at least 22; otherwise false.
 */
bool force_feedback_script_status_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                ForceFeedbackRuntimeMode mode, uint8_t *response,
                                                size_t length);

/**
 * @brief Encode the script timing and variable query response.
 *
 * Writes the 25 00 00 31 07 response prefix, four timing variables, and the eight writable script
 * variables in least-significant-byte-first order into the response buffer. The transport fills
 * the sequence byte at index 2 before transmission.
 *
 * @param[in] runtime Current script runtime containing variables.
 * @param[out] response Destination response buffer.
 * @param[in] length Number of writable bytes in response.
 * @return true when runtime and response are non-null and length is at least 53; otherwise false.
 */
bool force_feedback_script_values_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                uint8_t *response, size_t length);

#endif
