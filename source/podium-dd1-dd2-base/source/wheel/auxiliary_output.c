#include "wheel/auxiliary_output.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Report masks, latch flags, and scan output values for auxiliary encoding.
 *
 * The report uses three cumulative bands, while latched bands and encoded output use separate
 * bit positions.
 */
enum {
    AUXILIARY_REPORT_MASK = 0x01ff,      /**< Mask for the nine supported report bits. */
    AUXILIARY_LOW_BAND_MASK = 0x0007,    /**< Mask for the low cumulative band. */
    AUXILIARY_MIDDLE_BAND_MASK = 0x0038, /**< Mask for the middle cumulative band. */
    AUXILIARY_HIGH_BAND_MASK = 0x01c0,   /**< Mask for the high cumulative band. */
    AUXILIARY_LATCH_MIDDLE = 0x01,       /**< Latched-band bit for the middle band. */
    AUXILIARY_LATCH_HIGH = 0x02,         /**< Latched-band bit for the high band. */
    AUXILIARY_LATCH_LOW = 0x04,          /**< Latched-band bit for the low band. */
    AUXILIARY_OUTPUT_LOW = 0x01,         /**< Scan-output bit for the low band. */
    AUXILIARY_OUTPUT_MIDDLE = 0x40,      /**< Scan-output bit for the middle band. */
    AUXILIARY_OUTPUT_HIGH = 0x10,        /**< Scan-output bit for the high band. */
};

/**
 * @brief Maps retained auxiliary bands to scan output bits.
 *
 * Converts the low, middle, and high band latches to their attached-wheel output positions.
 *
 * @param[in] latched_bands Retained band flags.
 * @return Encoded scan output bits.
 */
static uint8_t latched_output(uint8_t latched_bands) {
    uint8_t encoded = 0;
    if ((latched_bands & AUXILIARY_LATCH_LOW) != 0) {
        encoded |= AUXILIARY_OUTPUT_LOW;
    }
    if ((latched_bands & AUXILIARY_LATCH_MIDDLE) != 0) {
        encoded |= AUXILIARY_OUTPUT_MIDDLE;
    }
    if ((latched_bands & AUXILIARY_LATCH_HIGH) != 0) {
        encoded |= AUXILIARY_OUTPUT_HIGH;
    }
    return encoded;
}

/**
 * @brief Combines active report bands and retained band latches.
 *
 * Sets each scan output independently when its corresponding report band or latch is active.
 *
 * @param[in] report Nine-bit shared output report.
 * @param[in] latched_bands Retained band flags.
 * @return Combined scan output bits.
 */
static uint8_t encode_combined(uint16_t report, uint8_t latched_bands) {
    uint8_t encoded = latched_output(latched_bands);
    if ((report & AUXILIARY_LOW_BAND_MASK) != 0) {
        encoded |= AUXILIARY_OUTPUT_LOW;
    }
    if ((report & AUXILIARY_MIDDLE_BAND_MASK) != 0) {
        encoded |= AUXILIARY_OUTPUT_MIDDLE;
    }
    if ((report & AUXILIARY_HIGH_BAND_MASK) != 0) {
        encoded |= AUXILIARY_OUTPUT_HIGH;
    }
    return encoded;
}

/**
 * @brief Encodes a cumulative nine-bit auxiliary pattern.
 *
 * Maps the nine supported cumulative report patterns to their attached-wheel scan codes.
 *
 * @param[in] report Nine-bit shared output report.
 * @return Pattern scan code, or zero when the report is not a supported pattern.
 */
static uint8_t encode_code(uint16_t report) {
    switch (report) {
    case 0x0100:
    case 0x0180:
    case 0x01c0:
        return 0x50;
    case 0x01e0:
    case 0x01f0:
    case 0x01f8:
        return 0x40;
    case 0x01fc:
    case 0x01fe:
        return 0x01;
    case 0x01ff:
        return 0x51;
    default:
        return 0;
    }
}

/**
 * @brief Selects the first active auxiliary band.
 *
 * Gives the low band priority over the middle and high bands. A complete low band selects the
 * combined low-and-high scan code.
 *
 * @param[in] report Nine-bit shared output report.
 * @param[in] latched_bands Retained band flags.
 * @return Exclusive scan output code.
 */
static uint8_t encode_exclusive(uint16_t report, uint8_t latched_bands) {
    if ((report & AUXILIARY_LOW_BAND_MASK) != 0 || (latched_bands & AUXILIARY_LATCH_LOW) != 0) {
        return (report & AUXILIARY_LOW_BAND_MASK) == AUXILIARY_LOW_BAND_MASK ? 0x51 : 0x01;
    }
    if ((report & AUXILIARY_MIDDLE_BAND_MASK) != 0 ||
        (latched_bands & AUXILIARY_LATCH_MIDDLE) != 0) {
        return 0x40;
    }
    if ((report & AUXILIARY_HIGH_BAND_MASK) != 0 || (latched_bands & AUXILIARY_LATCH_HIGH) != 0) {
        return 0x50;
    }
    return 0;
}

/**
 * @brief Encodes the attached-wheel auxiliary scan output.
 *
 * Maps the low, middle, and high bands of the nine-bit shared output report to the attached-wheel
 * scan lines. Code mode recognizes the nine cumulative patterns, while exclusive mode selects the
 * first active band. Latched bands augment either normal or code output, and a disabled output is
 * always clear.
 *
 * @param[in] output Auxiliary report and encoding policy.
 * @return Encoded auxiliary scan byte, or zero when output is null or option one disables output.
 */
uint8_t wheel_auxiliary_output_encode(const WheelAuxiliaryOutput *output) {
    if (output == NULL || output->option == 1) {
        return 0;
    }

    uint16_t report = output->report & AUXILIARY_REPORT_MASK;
    if (output->exclusive_mode) {
        return encode_exclusive(report, output->latched_bands);
    }
    if (output->code_mode) {
        return encode_code(report) | latched_output(output->latched_bands);
    }
    return encode_combined(report, output->latched_bands);
}
