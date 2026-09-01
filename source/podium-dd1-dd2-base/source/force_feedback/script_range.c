#include "force_feedback/script_range.h"

/** @brief Single-precision pi used by rotation scaling. */
static const float script_pi = 3.1415927f;

float force_feedback_script_range_evaluate(ForceFeedbackScriptRangeOperation operation, float lower,
                                           float upper, float value) {
    if (operation == FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE) {
        return (value - lower) / (upper - lower);
    }
    if (value <= lower) {
        return -1.0f;
    }
    if (value >= upper) {
        return 1.0f;
    }
    if (operation == FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED) {
        return (value - lower) / (upper - lower);
    }
    return 0.0f;
}

float force_feedback_script_rotation_scale(float value, uint8_t range_code,
                                           uint16_t extended_range) {
    int32_t selected_range =
        (int8_t)range_code > 125 ? (int32_t)extended_range : (int8_t)range_code;
    float range_scale = (float)selected_range * 5.0f;
    return value * script_pi * range_scale / 180.0f;
}
