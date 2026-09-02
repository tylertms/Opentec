#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_TICK_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_TICK_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/script_output.h"
#include "force_feedback/script_runtime.h"

/**
 * @brief Identifies the motor-output action selected for a script tick.
 *
 * Values correspond to the output policies defined below.
 */
typedef uint8_t ForceFeedbackScriptOutputPolicy;

/**
 * @brief Motor-output policies selected for one script tick.
 *
 * The policy tells the output path whether to leave the report unchanged, process zero script
 * force, or process script motion force.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE = 0, /**< Do not process or write a motor output. */
    FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO =
        1, /**< Process zero script force through the output path. */
    FORCE_FEEDBACK_SCRIPT_OUTPUT_MOTION =
        2, /**< Process script motion force through the output path. */
    FORCE_FEEDBACK_SCRIPT_OUTPUT_POSITION =
        3, /**< Process script position force through the output path. */
};

/**
 * @brief Script execution decision for one scheduled tick.
 *
 * Carries the selected motor-output policy and the fault event used for system-level reporting.
 */
typedef struct {
    ForceFeedbackScriptOutputPolicy output_policy; /**< Selected motor-output policy. */
    bool slot_faulted;   /**< Whether a slot fault reached the standard report path. */
    bool immediate_zero; /**< Whether zero output bypasses normal output processing. */
} ForceFeedbackScriptTickDecision;

/**
 * @brief Applied force-output result for one scheduled script tick.
 *
 * Reports the motor write, wheel travel-limit state, and slot fault after output processing.
 */
typedef struct {
    bool wrote_output;   /**< Whether the motor report was written. */
    bool outside_travel; /**< Whether enabled travel-limit processing reported an outside position.
                          */
    bool slot_faulted;   /**< Whether a slot fault reached the standard report path. */
} ForceFeedbackScriptTickResult;

/**
 * @brief Service one due script tick and select its motor-output policy.
 *
 * Advances scheduling and the applicable timing, active-script, and motion state when a tick is
 * due. Position-only and zero-output modes select zero-force processing; active host input also
 * selects zero-force processing, while ready or idle input permits script motion only when the
 * script selector remains zero. Expired host input selects an immediate primary-force clear without
 * executing scripts.
 *
 * @param[in,out] system Complete script runtime and scheduler state to update.
 * @param[in] now Current monotonic time in milliseconds.
 * @param[in] wheel_position Signed raw wheel-position sample.
 * @param[in] half_travel Positive raw wheel travel from center to either endpoint.
 * @return The selected output policy and slot-fault state.
 */
ForceFeedbackScriptTickDecision force_feedback_script_tick(ForceFeedbackScriptSystem *system,
                                                           uint32_t now, int32_t wheel_position,
                                                           uint32_t half_travel);

/**
 * @brief Service one script tick and conditionally update the motor report.
 *
 * Gives a pending position request priority, otherwise processes the selected script motion or zero
 * script force, including an immediate primary-force clear on an expired input. Non-immediate zero
 * force uses the normal processing path, and the report remains unchanged when no write is due.
 *
 * @param[in,out] system Complete script runtime and scheduler state to update.
 * @param[in,out] output_state Script smoothing and travel-limit state.
 * @param[in] now Current monotonic time in milliseconds.
 * @param[in] wheel_position Signed raw wheel-position sample.
 * @param[in] half_travel Positive raw wheel travel from center to either endpoint.
 * @param[in] config Smoothing, strength, ramp, range, and output limits.
 * @param[in,out] report Motor output report to update when a write is selected.
 * @return The motor-write, travel-limit, and slot-fault result; wrote_output is false when no write
 * was selected.
 * @pre system, output_state, config, and report point to valid objects.
 * @pre half_travel is nonzero when position output samples wheel position.
 */
ForceFeedbackScriptTickResult force_feedback_script_tick_output(
    ForceFeedbackScriptSystem *system, ForceFeedbackScriptOutputState *output_state, uint32_t now,
    int32_t wheel_position, uint32_t half_travel, const ForceFeedbackScriptOutputConfig *config,
    ForceOutputReport *report);

#endif
