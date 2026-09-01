#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OUTPUT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/filter.h"
#include "force_feedback/output_report.h"
#include "force_feedback/soft_stop.h"

/**
 * @brief Stateful processing data for script force output.
 *
 * The state retains force-smoothing history and travel-limit ramp state between output updates.
 */
typedef struct {
    ForceFilter filter;           /**< Force-smoothing state. */
    ForceSoftStopState soft_stop; /**< Wheel travel-limit state. */
} ForceFeedbackScriptOutputState;

/**
 * @brief Configuration used to convert script force into a motor report.
 *
 * The configuration combines smoothing, startup ramp, strength, available output, travel-limit,
 * and secondary-output settings used by the script output path.
 */
typedef struct {
    ForceSoftStopConfig soft_stop;   /**< Wheel travel-limit configuration. */
    uint16_t available_percent;      /**< Percentage of motor output currently available. */
    int32_t output_strength_percent; /**< Final base output-strength percentage. */
    uint8_t automatic_strength;      /**< Strength used when tuning_strength equals 101. */
    uint8_t ramp_percent;            /**< Startup ramp percentage. */
    uint8_t smoothing_intensity;     /**< Force-smoothing intensity. */
    int8_t tuning_strength;          /**< Signed script force strength setting. */
    bool secondary_output_disabled;  /**< Whether secondary output remains disabled. */
} ForceFeedbackScriptOutputConfig;

/**
 * @brief Convert normalized script motion into a signed force request.
 *
 * Treats an all-ones motion value as zero. Strengths through 100 apply directly, strength 101 uses
 * automatic_strength, and higher strengths map to strength times 10 minus 910. The scaled motion
 * uses a full-scale force magnitude of 65,535 and truncates toward zero.
 *
 * @param[in] motion Raw floating-point bits from script motion output 2.
 * @param[in] strength Signed tuning strength.
 * @param[in] automatic_strength Strength used when tuning strength equals 101.
 * @return Signed unfiltered force request.
 */
int32_t force_feedback_script_output_request(uint32_t motion, int8_t strength,
                                             uint8_t automatic_strength);

/**
 * @brief Apply the startup ramp to a filtered script force request.
 *
 * Multiplies the signed request by the ramp percentage and divides by 100 using integer
 * truncation.
 *
 * @param[in] filtered Signed filtered force request.
 * @param[in] ramp_percent Current startup ramp percentage.
 * @return Ramped force request.
 */
int32_t force_feedback_script_output_ramp(int32_t filtered, uint8_t ramp_percent);

/**
 * @brief Reset script force-output processing state.
 *
 * Clears force-smoothing history and wheel travel-limit ramp state.
 *
 * @param[out] state Script output state to clear.
 * @pre state points to a valid object.
 */
void force_feedback_script_output_init(ForceFeedbackScriptOutputState *state);

/**
 * @brief Apply one script motion value to a motor output report.
 *
 * Applies tuning strength, time-gated smoothing, startup ramp, wheel travel limiting, signed
 * magnitude clamping, available-output limiting, and final output strength in processing order.
 * Updates report direction and primary magnitude on every call. The secondary report
 * magnitude remains unchanged when secondary output is disabled.
 *
 * @param[in,out] state Script smoothing and travel-limit state.
 * @param[in] motion Raw floating-point bits from script motion output 2.
 * @param[in] position Centered wheel position.
 * @param[in] now_ms Current system time in milliseconds.
 * @param[in] config Strength, smoothing, ramp, range, and output limits.
 * @param[in,out] report Motor output report to update.
 * @return true when enabled travel-limit processing reports an outside position; false when the
 * position is inside the limit or secondary output is disabled.
 * @pre state, config, and report point to valid objects.
 */
bool force_feedback_script_output_apply(ForceFeedbackScriptOutputState *state, uint32_t motion,
                                        int32_t position, uint32_t now_ms,
                                        const ForceFeedbackScriptOutputConfig *config,
                                        ForceOutputReport *report);

/**
 * @brief Apply wheel position to a position-mode motor output report.
 *
 * Normalizes the negated wheel position by half travel, applies tuning strength and startup ramp,
 * limits the force at the configured travel boundary when secondary output is enabled, and scales
 * the result into the report without smoothing.
 *
 * @param[in,out] state Wheel travel-limit state.
 * @param[in] position Centered wheel position.
 * @param[in] half_travel Positive travel from center to either endpoint.
 * @param[in] now_ms Current system time in milliseconds.
 * @param[in] config Strength, ramp, range, and output limits.
 * @param[in,out] report Motor output report to update.
 * @return true when enabled travel-limit processing reports an outside position; false when the
 * position is inside the limit or secondary output is disabled.
 * @pre state, config, and report point to valid objects.
 * @pre half_travel is nonzero.
 */
bool force_feedback_script_position_output_apply(ForceFeedbackScriptOutputState *state,
                                                 int32_t position, uint32_t half_travel,
                                                 uint32_t now_ms,
                                                 const ForceFeedbackScriptOutputConfig *config,
                                                 ForceOutputReport *report);

#endif
