#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SAMPLE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/script_input.h"

/**
 * @brief Result of reading or interpolating a script sample.
 *
 * The value uses the raw 32-bit script representation, and writes_value controls whether the
 * caller may write the result to an operation destination.
 */
typedef struct {
    uint32_t value;    /**< Raw sample value or interpolated float representation. */
    bool writes_value; /**< Whether the result may be written to the destination. */
} ForceFeedbackScriptSampleResult;

/**
 * @brief Read one sample at a base-relative index.
 *
 * Adds base and offset with unsigned 32-bit wraparound and suppresses the destination write when
 * the resulting index is outside the sample table.
 *
 * @param[in] samples Script sample table.
 * @param[in] base Base sample index.
 * @param[in] offset Unsigned offset from base.
 * @return The raw sample value when in range; otherwise a suppressed-write result.
 * @pre samples points to a valid sample table.
 */
ForceFeedbackScriptSampleResult
force_feedback_script_sample_read(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                  uint32_t offset);

/**
 * @brief Read one sample at a wrapped base-relative index.
 *
 * Reduces value modulo period, adds the result to base with unsigned 32-bit wraparound, and
 * suppresses the destination write when period is zero or the final index is outside the table.
 *
 * @param[in] samples Script sample table.
 * @param[in] base Base sample index.
 * @param[in] value Unsigned value to wrap.
 * @param[in] period Wrapping period; zero suppresses the destination write.
 * @return The raw sample value when in range; otherwise a suppressed-write result.
 * @pre samples points to a valid sample table.
 */
ForceFeedbackScriptSampleResult
force_feedback_script_sample_read_wrapped(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                          uint32_t value, uint32_t period);

/**
 * @brief Interpolate a script sample curve at a target value.
 *
 * Reads point_count x/y pairs beginning at base, extrapolates outside the first or last x value,
 * and interpolates the matching segment for an interior target. Fewer than two points, a curve that
 * exceeds the sample table, or a target without a matching segment suppresses the write.
 *
 * @param[in] samples Script sample table.
 * @param[in] base Index of the first x value.
 * @param[in] point_count Number of x/y point pairs.
 * @param[in] target Floating-point x value to evaluate.
 * @return The interpolated or extrapolated raw float value when the curve is valid; otherwise a
 * suppressed-write result.
 * @pre samples points to a valid sample table.
 */
ForceFeedbackScriptSampleResult
force_feedback_script_sample_interpolate(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                         uint32_t point_count, float target);

#endif
