#include "force_feedback/output_report.h"

#include <stdint.h>

/**
 * @brief Encodes the final direction and two force magnitudes sent to the motor controller.
 * @param report Final force-output fields after limits and strength scaling are applied.
 * @param output Five-byte little-endian force-output payload.
 */
void force_output_report_encode(const ForceOutputReport *report,
                                uint8_t output[FORCE_OUTPUT_REPORT_SIZE]) {
    output[0] = report->positive_direction;
    output[1] = (uint8_t)report->primary_magnitude;
    output[2] = (uint8_t)(report->primary_magnitude >> 8);
    output[3] = (uint8_t)report->secondary_magnitude;
    output[4] = (uint8_t)(report->secondary_magnitude >> 8);
}
