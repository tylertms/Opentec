#include "force_feedback/output_scale.h"

#include <stdint.h>

/**
 * @brief Separates and clamps a signed force command.
 *
 * Reports nonnegative values as the positive direction and limits the absolute magnitude to the
 * 16-bit force-command range.
 *
 * @param[in] force Signed force command.
 * @param[out] positive_direction True for zero and positive commands.
 * @return Absolute force magnitude limited to 65,535.
 */
static uint16_t split_force(int32_t force, bool *positive_direction) {
    *positive_direction = force >= 0;
    uint32_t magnitude = force < 0 ? (uint32_t)-(int64_t)force : (uint32_t)force;
    return magnitude > UINT16_MAX ? UINT16_MAX : (uint16_t)magnitude;
}

void force_output_scale_apply(int32_t force, int32_t secondary_magnitude, ForceOutputScale scale,
                              ForceOutputReport *report) {
    uint16_t primary = split_force(force, &report->positive_direction);
    uint32_t available = (uint32_t)UINT16_MAX * scale.available_percent / 100;
    available = available * scale.tuning_strength_percent / 100;

    if (available <= primary) {
        primary = (uint16_t)available;
    } else if (available <= (uint32_t)secondary_magnitude) {
        secondary_magnitude = (int32_t)available;
    }

    report->primary_magnitude = (uint16_t)((int64_t)primary * scale.output_strength_percent / 100);
    if (!scale.secondary_output_disabled) {
        report->secondary_magnitude =
            (uint16_t)((int64_t)secondary_magnitude * scale.output_strength_percent / 100);
    }
}
