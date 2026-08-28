#include "force_feedback/script_output.h"

#include <stdint.h>

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
