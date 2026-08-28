#include "force_feedback/script_scheduler.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Select the next force-feedback script scheduling action.
 *
 * Position input schedules an idle tick only after the local deadline and moves that deadline to
 * the current time. Active and ready input require a nonzero host period and host deadline. A host
 * deadline at or behind the current script sample counter reports expiration; otherwise a host
 * tick is scheduled only after the local deadline and advances it by the host period. Other input
 * statuses do not schedule work.
 *
 * @param[in,out] scheduler Local millisecond deadline for the next script tick.
 * @param[in] inputs Current host input status, period, and script-sample deadline.
 * @param[in] current_sample_count Number of completed script ticks.
 * @param[in] now Current monotonic time in milliseconds.
 * @return No action, an idle tick, a host-scheduled tick, or host-input expiration.
 * @pre scheduler and inputs point to valid objects.
 */
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
