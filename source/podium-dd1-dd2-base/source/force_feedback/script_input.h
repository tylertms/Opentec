#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INPUT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    FORCE_FEEDBACK_SCRIPT_PACKET_SIZE = 64,
    FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT = 512,
    FORCE_FEEDBACK_SCRIPT_SAMPLE_UPDATE_COUNT = 10,
    FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT = 3,
};

typedef uint8_t ForceFeedbackScriptInputStatus;

enum {
    FORCE_FEEDBACK_SCRIPT_INPUT_POSITION = 0,
    FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE = 1,
    FORCE_FEEDBACK_SCRIPT_INPUT_READY = 0xf0,
    FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED = UINT8_MAX,
};

typedef struct {
    uint32_t values[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT];
} ForceFeedbackScriptSamples;

typedef struct {
    ForceFeedbackScriptInputStatus status;
    uint32_t value;
    uint32_t duration;
} ForceFeedbackScriptInputSlot;

typedef struct {
    ForceFeedbackScriptInputStatus status;
    uint32_t deadline;
    uint16_t sample_count;
    ForceFeedbackScriptInputSlot slots[FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT];
    uint32_t position_value;
} ForceFeedbackScriptInputs;

void force_feedback_script_samples_init(ForceFeedbackScriptSamples *samples);
bool force_feedback_script_samples_apply(ForceFeedbackScriptSamples *samples, const uint8_t *packet,
                                         size_t length);
void force_feedback_script_inputs_init(ForceFeedbackScriptInputs *inputs);
bool force_feedback_script_inputs_apply(ForceFeedbackScriptInputs *inputs,
                                        uint32_t current_sample_count, const uint8_t *packet,
                                        size_t length);

#endif
