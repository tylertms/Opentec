#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SERVICE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SERVICE_H

#include <stdbool.h>

#include "force_feedback/script_executor.h"
#include "force_feedback/script_store.h"

/**
 * @brief Execute every active force-feedback script slot once.
 *
 * Visits slots in ascending order. For each active slot, selects its stored script, resets an aged
 * timing counter, snapshots the counter, executes the script, and updates its execution metrics
 * while the clock attributes elapsed ticks to that slot.
 *
 * @param[in,out] runtime Script values, samples, outputs, axes, and slot state to update.
 * @param[in] store Uploaded script byte sequences and allocation records.
 * @param[in,out] clock Timing counters and active-execution markers to update.
 * @return true when an active slot fault reaches the standard status-report path; otherwise false.
 * @pre Every active runtime slot has a corresponding valid allocation in store.
 */
bool force_feedback_script_service_run(ForceFeedbackScriptRuntime *runtime,
                                       const ForceFeedbackScriptStore *store,
                                       ForceFeedbackScriptClock *clock);

/**
 * @brief Runs one script-service pass with optional execution-state tracking.
 *
 * @param[in,out] runtime Script runtime to advance.
 * @param[in] store Script storage containing the selected program.
 * @param[in,out] clock Script clock to advance.
 * @param[in] track_execution True to publish execution-state transitions.
 * @return True when script execution remains active; otherwise false.
 */
bool force_feedback_script_service_run_tracked(ForceFeedbackScriptRuntime *runtime,
                                               const ForceFeedbackScriptStore *store,
                                               ForceFeedbackScriptClock *clock,
                                               bool track_execution);

#endif
