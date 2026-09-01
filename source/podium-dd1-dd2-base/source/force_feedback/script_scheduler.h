#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SCHEDULER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SCHEDULER_H

#include <stdint.h>

#include "force_feedback/script_input.h"

/**
 * @brief Identifies the scheduling action returned for a script tick.
 *
 * Values distinguish no work, idle work, host work, and expired host input.
 */
typedef uint8_t ForceFeedbackScriptSchedule;

/**
 * @brief Script scheduling actions.
 *
 * The scheduler reports whether the caller should service an idle or host tick.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE = 0, /**< No script tick is due. */
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_IDLE = 1, /**< A position-mode idle tick is due. */
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST = 2, /**< A host-input script tick is due. */
    FORCE_FEEDBACK_SCRIPT_SCHEDULE_EXPIRED =
        3, /**< Host input has reached or passed its script deadline. */
};

/**
 * @brief Deadline state for script scheduling.
 *
 * The deadline is compared with the current monotonic millisecond time before a tick is selected.
 */
typedef struct {
    uint32_t deadline; /**< Next local millisecond deadline. */
} ForceFeedbackScriptScheduler;

/**
 * @brief Select the next force-feedback script scheduling action.
 *
 * Position input selects idle work only when now is greater than the local deadline. Active and
 * ready input return no work until their sample period and input deadline are nonzero, report
 * expiration when the input deadline is at or below the current sample count, and otherwise select
 * host work only when now is greater than the local deadline, advancing it by the sample period
 * from now.
 *
 * @param[in,out] scheduler Local deadline state to update when work is selected.
 * @param[in] inputs Current host input status, period, and script-sample deadline.
 * @param[in] current_sample_count Number of completed script ticks.
 * @param[in] now Current monotonic time in milliseconds.
 * @return The next scheduling action.
 */
ForceFeedbackScriptSchedule
force_feedback_script_scheduler_step(ForceFeedbackScriptScheduler *scheduler,
                                     const ForceFeedbackScriptInputs *inputs,
                                     uint32_t current_sample_count, uint32_t now);

#endif
