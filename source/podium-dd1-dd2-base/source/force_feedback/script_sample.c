#include "force_feedback/script_sample.h"

#include <stdint.h>

typedef union {
    float number;
    uint32_t bits;
} ScriptSampleValue;

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

/**
 * @brief Read a script sample at a base-relative index.
 *
 * Adds the unsigned 32-bit base and offset with wraparound. A resulting index from 0 through 511
 * writes the corresponding raw sample payload. A larger index suppresses the destination write.
 *
 * @param[in] samples Script sample table containing 512 raw values.
 * @param[in] base Base sample index.
 * @param[in] offset Unsigned offset from the base index.
 * @return The raw sample value and whether the VM writes it to the destination.
 * @pre samples points to an initialized script sample table.
 */
ForceFeedbackScriptSampleResult
force_feedback_script_sample_read(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                  uint32_t offset) {
    return sample_at(samples, base + offset);
}

/**
 * @brief Read a script sample using a wrapped offset.
 *
 * Reduces the unsigned value modulo the period, then adds it to the base with 32-bit wraparound. A
 * zero period or final index above 511 suppresses the destination write. Otherwise the operation
 * writes the corresponding raw sample payload.
 *
 * @param[in] samples Script sample table containing 512 raw values.
 * @param[in] base Base sample index.
 * @param[in] value Unsigned value to wrap within the period.
 * @param[in] period Nonzero wrapping period.
 * @return The raw sample value and whether the VM writes it to the destination.
 * @pre samples points to an initialized script sample table.
 */
ForceFeedbackScriptSampleResult
force_feedback_script_sample_read_wrapped(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                          uint32_t value, uint32_t period) {
    return period == 0 ? (ForceFeedbackScriptSampleResult){0}
                       : sample_at(samples, base + value % period);
}

static float sample_number(const ForceFeedbackScriptSamples *samples, uint32_t index) {
    return (ScriptSampleValue){.bits = samples->values[index]}.number;
}

static ForceFeedbackScriptSampleResult interpolated_result(float value) {
    return (ForceFeedbackScriptSampleResult){
        .value = (ScriptSampleValue){.number = value}.bits,
        .writes_value = true,
    };
}

static float interpolate_segment(float previous_x, float previous_y, float current_x,
                                 float current_y, float target) {
    float slope = (current_y - previous_y) / (current_x - previous_x);
    return previous_y + slope * (target - previous_x);
}

/**
 * @brief Interpolate a script sample curve.
 *
 * Reads point_count pairs of floating-point x and y payloads beginning at base. A target below the
 * first x extrapolates the first segment, and a target above the last x extrapolates the last
 * segment. Targets within the curve scan segments from last to first and interpolate the first
 * bracket where previous_x <= target <= current_x. Fewer than two points, a curve extending beyond
 * the 512-value table, or a curve without a matching bracket suppresses the destination write.
 *
 * @param[in] samples Script sample table containing 512 raw values.
 * @param[in] base Index of the first x payload.
 * @param[in] point_count Number of consecutive x/y point pairs.
 * @param[in] target Floating-point x value to evaluate.
 * @return The raw interpolated float payload and whether the VM writes it.
 * @pre samples points to an initialized script sample table.
 */
ForceFeedbackScriptSampleResult
force_feedback_script_sample_interpolate(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                         uint32_t point_count, float target) {
    if (base >= FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT || point_count <= 1 ||
        point_count > (FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT - base) / 2) {
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
