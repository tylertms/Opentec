#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_REPORT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_REPORT_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of bytes in the encoded force-output payload. */
enum {
    FORCE_OUTPUT_REPORT_SIZE = 5 /**< Number of bytes in an encoded force-output payload. */
};

/**
 * @brief Direction and magnitudes for one motor force-output payload.
 *
 * The report keeps one shared direction bit and separate 16-bit primary and secondary magnitudes
 * before the values are serialized for the motor controller.
 */
typedef struct {
    bool positive_direction;      /**< True when the force direction is positive, including zero. */
    uint16_t primary_magnitude;   /**< Primary force magnitude in the 16-bit report range. */
    uint16_t secondary_magnitude; /**< Secondary force magnitude in the 16-bit report range. */
} ForceOutputReport;

/**
 * @brief Encodes a force-output report into its five-byte wire payload.
 *
 * Writes the direction byte followed by the primary and secondary magnitudes in little-endian
 * order for transmission to the motor controller.
 *
 * @param[in] report Force-output direction and magnitudes to serialize.
 * @param[out] output Five-byte destination payload.
 */
void force_output_report_encode(const ForceOutputReport *report,
                                uint8_t output[FORCE_OUTPUT_REPORT_SIZE]);

#endif
