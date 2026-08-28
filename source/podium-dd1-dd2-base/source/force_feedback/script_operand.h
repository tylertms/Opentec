#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERAND_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_control.h"
#include "force_feedback/script_input.h"

enum {
    FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT = 12,
    FORCE_FEEDBACK_SCRIPT_WRITABLE_VARIABLE_COUNT = 8,
    FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT = 8,
    FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT = 10,
};

typedef struct {
    uint32_t variables[FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT];
    uint32_t motion[FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT];
    uint32_t axes[FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT];
    ForceFeedbackScriptSamples samples;
    ForceFeedbackScriptSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT];
    uint8_t active_slot;
} ForceFeedbackScriptRuntime;

bool force_feedback_script_operand_read(const ForceFeedbackScriptRuntime *runtime,
                                        const uint8_t *script, size_t length, size_t *cursor,
                                        uint32_t *value);
bool force_feedback_script_operand_write(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                         size_t length, size_t *cursor, uint32_t value,
                                         bool commit);

#endif
