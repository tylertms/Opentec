#include "force_feedback/script_output.h"

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_scale.h"

/**
 * @brief Provides numeric and raw-bit views of a script output value.
 *
 * Output conversion preserves raw script bits while interpreting motion as a floating-point
 * value.
 */
typedef union {
    float number;  /**< Single-precision numeric view. */
    uint32_t bits; /**< Raw 32-bit representation. */
} OutputValue;

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

int32_t force_feedback_script_output_ramp(int32_t filtered, uint8_t ramp_percent) {
    return (int32_t)((int64_t)filtered * ramp_percent / 100);
}

void force_feedback_script_output_init(ForceFeedbackScriptOutputState *state) {
    *state = (ForceFeedbackScriptOutputState){0};
}

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
