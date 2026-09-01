#include "force_feedback/script_scheduler.h"

#include <stddef.h>
#include <stdint.h>

ForceFeedbackScriptSchedule
force_feedback_script_scheduler_step(ForceFeedbackScriptScheduler *scheduler,
                                     const ForceFeedbackScriptInputs *inputs,
                                     uint32_t current_sample_count, uint32_t now) {
    if (scheduler == NULL || inputs == NULL) {
        return FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE;
    }
    if (inputs->status == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION) {
        if (now > scheduler->deadline) {
            scheduler->deadline = now;
            return FORCE_FEEDBACK_SCRIPT_SCHEDULE_IDLE;
        }
        return FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE;
    }
    if (inputs->status != FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE &&
        inputs->status != FORCE_FEEDBACK_SCRIPT_INPUT_READY) {
        return FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE;
    }
    if (inputs->sample_count == 0 || inputs->deadline == 0) {
        return FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE;
    }
    if (inputs->deadline <= current_sample_count) {
        return FORCE_FEEDBACK_SCRIPT_SCHEDULE_EXPIRED;
    }
    if (now <= scheduler->deadline) {
        return FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE;
    }
    scheduler->deadline = now + inputs->sample_count;
    return FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST;
}
