#include "force_feedback/script_sample.h"

#include <stdint.h>

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
