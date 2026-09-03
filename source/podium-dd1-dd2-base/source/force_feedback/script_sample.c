#include "force_feedback/script_sample.h"

#include <stdint.h>

/**
 * @brief Provides numeric and raw-bit views of a script sample value.
 *
 * Sample operations preserve raw table values while converting curve coordinates to and from
 * floating-point values.
 */
typedef union {
    float number;  /**< Single-precision numeric view. */
    uint32_t bits; /**< Raw 32-bit representation. */
} ScriptSampleValue;

/**
 * @brief Reads one raw script sample.
 *
 * Accepts indexes from 0 through 511 and suppresses the destination write for larger indexes.
 *
 * @param[in] samples Script sample table.
 * @param[in] index Sample index.
 * @return The raw sample value and whether the destination write is enabled.
 */
static ForceFeedbackScriptSampleResult sample_at(const ForceFeedbackScriptSamples *samples,
                                                 uint32_t index) {
    if (index >= FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT) {
        return (ForceFeedbackScriptSampleResult){0};
    }
    return (ForceFeedbackScriptSampleResult){
        .value = samples->values[index],
        .writes_value = true,
    };
}

ForceFeedbackScriptSampleResult
force_feedback_script_sample_read(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                  uint32_t offset) {
    return sample_at(samples, base + offset);
}

ForceFeedbackScriptSampleResult
force_feedback_script_sample_read_wrapped(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                          uint32_t value, uint32_t period) {
    return period == 0 ? (ForceFeedbackScriptSampleResult){0}
                       : sample_at(samples, base + value % period);
}

/**
 * @brief Reads one script sample as floating point.
 *
 * Interprets the selected raw sample payload without changing its bits.
 *
 * @param[in] samples Script sample table.
 * @param[in] index In-range sample index.
 * @return The represented single-precision value.
 */
static float sample_number(const ForceFeedbackScriptSamples *samples, uint32_t index) {
    return (ScriptSampleValue){.bits = samples->values[index]}.number;
}

/**
 * @brief Creates a writable interpolated-sample result.
 *
 * Stores the raw representation of the interpolated value and enables the destination write.
 *
 * @param[in] value Interpolated floating-point value.
 * @return A writable sample result containing the value.
 */
static ForceFeedbackScriptSampleResult interpolated_result(float value) {
    return (ForceFeedbackScriptSampleResult){
        .value = (ScriptSampleValue){.number = value}.bits,
        .writes_value = true,
    };
}

/**
 * @brief Interpolates or extrapolates one line segment.
 *
 * Calculates the segment slope and evaluates it at the target x coordinate.
 *
 * @param[in] previous_x First x coordinate.
 * @param[in] previous_y First y coordinate.
 * @param[in] current_x Second x coordinate.
 * @param[in] current_y Second y coordinate.
 * @param[in] target Target x coordinate.
 * @return The corresponding y value on the segment.
 */
static float interpolate_segment(float previous_x, float previous_y, float current_x,
                                 float current_y, float target) {
    float slope = (current_y - previous_y) / (current_x - previous_x);
    return previous_y + slope * (target - previous_x);
}

ForceFeedbackScriptSampleResult
force_feedback_script_sample_interpolate(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                         uint32_t point_count, float target) {
    if (base >= FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT || point_count <= 1 ||
        point_count > (FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT - 1u - base) / 2u) {
        return (ForceFeedbackScriptSampleResult){0};
    }

    uint32_t end = base + point_count * 2;
    float first_x = sample_number(samples, base);
    float first_y = sample_number(samples, base + 1);
    if (first_x > target) {
        return interpolated_result(interpolate_segment(first_x, first_y,
                                                       sample_number(samples, base + 2),
                                                       sample_number(samples, base + 3), target));
    }

    float last_x = sample_number(samples, end - 2);
    if (last_x < target) {
        return interpolated_result(interpolate_segment(sample_number(samples, end - 4),
                                                       sample_number(samples, end - 3), last_x,
                                                       sample_number(samples, end - 1), target));
    }

    for (uint32_t current = end - 2; current > base; current -= 2) {
        float current_x = sample_number(samples, current);
        float previous_x = sample_number(samples, current - 2);
        if (current_x >= target && previous_x <= target) {
            return interpolated_result(
                interpolate_segment(previous_x, sample_number(samples, current - 1), current_x,
                                    sample_number(samples, current + 1), target));
        }
    }
    return (ForceFeedbackScriptSampleResult){0};
}
