#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_SCALE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_SCALE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"

/**
 * @brief Output limits and gates used to scale motor force magnitudes.
 *
 * Available output and tuning strength determine the primary limit. Base output strength scales
 * the primary and enabled secondary magnitudes, while the secondary gate controls its report field.
 */
typedef struct {
    uint16_t available_percent; /**< Percentage of the 16-bit output range currently available. */
    int8_t
        tuning_strength_percent; /**< Tuning percentage applied to the available primary range. */
    int32_t output_strength_percent; /**< Base output-strength percentage for report magnitudes. */
    bool secondary_output_disabled; /**< Whether the secondary report field retains its prior value.
                                     */
} ForceOutputScale;

/**
 * @brief Applies available-output and strength scaling to motor force magnitudes.
 *
 * Splits the signed primary force into direction and magnitude, computes the available range with
 * a 32-bit unsigned multiply/divide, narrows it to 16 bits before tuning scaling, applies tuning
 * with a signed 32-bit multiply/divide, narrows the tuned range to 16 bits, caps the secondary
 * magnitude at that range when the primary is below it, and applies base output strength to each
 * enabled report field.
 *
 * @param[in] force Signed primary force command.
 * @param[in] secondary_magnitude Nonnegative secondary force magnitude.
 * @param[in] scale Available-output, tuning, base-strength, and secondary-output settings.
 * @param[in,out] report Output direction and magnitudes to update.
 */
void force_output_scale_apply(int32_t force, int32_t secondary_magnitude, ForceOutputScale scale,
                              ForceOutputReport *report);

#endif
