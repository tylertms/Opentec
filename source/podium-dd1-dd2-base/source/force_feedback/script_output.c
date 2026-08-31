#include "force_feedback/script_output.h"

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_scale.h"

typedef union {
    float number;
    uint32_t bits;
} OutputValue;

/**
 * @brief Convert normalized script motion into a signed force request.
 *
 * Treats an all-ones motion value as zero. Strengths through 100 apply directly as percentages,
 * strength 101 uses the automatic strength, and higher strengths use the extended percentage
 * curve strength times 10 minus 910. The scaled motion is converted with a full-scale magnitude
 * of 65,535 and truncated toward zero.
 *
 * @param[in] motion Raw floating-point bits from script motion output 2.
 * @param[in] strength Signed tuning strength.
 * @param[in] automatic_strength Runtime strength selected by tuning strength 101.
 * @return Signed unfiltered force request.
 */
int32_t force_feedback_script_output_request(uint32_t motion, int8_t strength,
                                             uint8_t automatic_strength) {
    float value = motion == UINT32_MAX ? 0.0f : (OutputValue){.bits = motion}.number;
    int32_t percent = strength;
    if (strength == 101) {
        percent = automatic_strength;
    } else if (strength > 101) {
        percent = (int32_t)strength * 10 - 910;
    }
    float scaled_strength = (float)percent / 100.0f;
    return (int32_t)(value * scaled_strength * 65535.0f);
}

/**
 * @brief Apply the force-feedback startup ramp to a filtered script request.
 *
 * Multiplies the signed filtered request by the current ramp percentage and divides by 100 with
 * integer truncation toward zero.
 *
 * @param[in] filtered Signed filtered force request.
 * @param[in] ramp_percent Current startup ramp percentage.
 * @return Ramped force request for position limiting and command clamping.
 */
int32_t force_feedback_script_output_ramp(int32_t filtered, uint8_t ramp_percent) {
    return (int32_t)((int64_t)filtered * ramp_percent / 100);
}

/**
 * @brief Reset force-script output processing state.
 *
 * Clears the smoothing history and wheel-range end-stop ramp.
 *
 * @param[out] state Script output state to clear.
 */
void force_feedback_script_output_init(ForceFeedbackScriptOutputState *state) {
    *state = (ForceFeedbackScriptOutputState){0};
}

/**
 * @brief Convert one force-script motion value into the live motor output report.
 *
 * Applies tuning strength, time-gated smoothing, the startup ramp, wheel-range limiting, signed
 * magnitude clamping, available-output limiting, and final output strength in firmware order.
 * The secondary magnitude is preserved while secondary output is disabled.
 *
 * @param[in,out] state Script smoothing and wheel-range end-stop state.
 * @param[in] motion Raw floating-point bits from script motion output 2.
 * @param[in] position Centered wheel position.
 * @param[in] now_ms Current system time in milliseconds.
 * @param[in] config Current smoothing, strength, ramp, range, and output limits.
 * @param[in,out] report Motor output report to update.
 * @return True when the centered wheel position is outside the configured travel limit.
 */
bool force_feedback_script_output_apply(ForceFeedbackScriptOutputState *state, uint32_t motion,
                                        int32_t position, uint32_t now_ms,
                                        const ForceFeedbackScriptOutputConfig *config,
                                        ForceOutputReport *report) {
    force_filter_configure(&state->filter, config->smoothing_intensity);
    int32_t force = force_feedback_script_output_request(motion, config->tuning_strength,
                                                         config->automatic_strength);
    force = force_filter_update(&state->filter, force, now_ms);
    force = force_feedback_script_output_ramp(force, config->ramp_percent);
    ForceSoftStopResult limited =
        force_soft_stop_update(&state->soft_stop, &config->soft_stop, position, force,
                               config->secondary_output_disabled, now_ms);
    ForceOutputScale scale = {
        .available_percent = config->available_percent,
        .tuning_strength_percent = config->tuning_strength,
        .output_strength_percent = config->output_strength_percent,
        .secondary_output_disabled = config->secondary_output_disabled,
    };
    force_output_scale_apply(limited.force, 0, scale, report);
    return limited.outside_travel;
}

/**
 * @brief Converts wheel position into the position-mode force-script output.
 *
 * Normalizes centered position against half travel, applies tuning strength and the startup ramp,
 * limits force at the configured travel boundary, and scales the result into the motor report.
 * Position mode intentionally bypasses motion-output smoothing.
 *
 * @param[in,out] state Wheel-range end-stop state.
 * @param[in] position Centered wheel position.
 * @param[in] half_travel Positive half-range used to normalize position.
 * @param[in] now_ms Current system time in milliseconds.
 * @param[in] config Current strength, ramp, range, and output limits.
 * @param[in,out] report Motor output report to update.
 * @return True when the centered wheel position is outside the configured travel limit.
 */
bool force_feedback_script_position_output_apply(ForceFeedbackScriptOutputState *state,
                                                 int32_t position, uint32_t half_travel,
                                                 uint32_t now_ms,
                                                 const ForceFeedbackScriptOutputConfig *config,
                                                 ForceOutputReport *report) {
    float normalized = -(float)position / (float)half_travel;
    int32_t force =
        force_feedback_script_output_request((OutputValue){.number = normalized}.bits,
                                             config->tuning_strength, config->automatic_strength);
    force = force_feedback_script_output_ramp(force, config->ramp_percent);
    ForceSoftStopResult limited =
        force_soft_stop_update(&state->soft_stop, &config->soft_stop, position, force,
                               config->secondary_output_disabled, now_ms);
    ForceOutputScale scale = {
        .available_percent = config->available_percent,
        .tuning_strength_percent = config->tuning_strength,
        .output_strength_percent = config->output_strength_percent,
        .secondary_output_disabled = config->secondary_output_disabled,
    };
    force_output_scale_apply(limited.force, 0, scale, report);
    return limited.outside_travel;
}
