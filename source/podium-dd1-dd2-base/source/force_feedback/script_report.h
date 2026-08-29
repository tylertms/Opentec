#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_REPORT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

enum {
    FORCE_FEEDBACK_SCRIPT_AXIS_REPORT_COUNT = 8,
    FORCE_FEEDBACK_SCRIPT_AXES_RESPONSE_SIZE = 46,
    FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_COUNT = 10,
    FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_LAST_FIRST = 501,
    FORCE_FEEDBACK_SCRIPT_SAMPLES_RESPONSE_SIZE = 47,
    FORCE_FEEDBACK_SCRIPT_SLOT_REPORT_LAST = 14,
    FORCE_FEEDBACK_SCRIPT_SLOT_REPORT_EMPTY = 15,
    FORCE_FEEDBACK_SCRIPT_SLOT_RESPONSE_SIZE = 39,
    FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE = 22,
    FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE = 53,
};

uint8_t force_feedback_script_report_sequence_take(uint8_t *next_sequence);
bool force_feedback_script_axes_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t *response, size_t length);
bool force_feedback_script_samples_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                 uint16_t first_sample, uint8_t *response,
                                                 size_t length);
bool force_feedback_script_slot_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                              uint8_t slot, uint8_t *response, size_t length);
bool force_feedback_script_status_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                ForceFeedbackRuntimeMode mode, uint8_t *response,
                                                size_t length);
bool force_feedback_script_values_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                uint8_t *response, size_t length);

#endif
