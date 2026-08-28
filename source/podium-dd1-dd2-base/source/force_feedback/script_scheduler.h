#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SCHEDULER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SCHEDULER_H

#include <stdint.h>

#include "force_feedback/script_input.h"

typedef uint8_t ForceFeedbackScriptSchedule;

enum {
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE = 0,
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_IDLE = 1,
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST = 2,
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_EXPIRED = 3,
};

typedef struct {
    uint32_t deadline;
} ForceFeedbackScriptScheduler;

ForceFeedbackScriptSchedule
force_feedback_script_scheduler_step(ForceFeedbackScriptScheduler *scheduler,
                                     const ForceFeedbackScriptInputs *inputs,
                                     uint32_t current_sample_count, uint32_t now);

#endif
