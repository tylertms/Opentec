#include "force_feedback/output_report.h"

#include <stdint.h>

void force_output_report_encode(const ForceOutputCommand *command, uint16_t secondary_magnitude,
                                uint8_t output[FORCE_OUTPUT_REPORT_SIZE]) {
    uint16_t primary = command->active != 0 ? command->magnitude : 0;
    uint16_t secondary = command->active != 0 ? secondary_magnitude : 0;

    output[0] = command->active != 0 && command->negative == 0;
    output[1] = (uint8_t)primary;
    output[2] = (uint8_t)(primary >> 8);
    output[3] = (uint8_t)secondary;
    output[4] = (uint8_t)(secondary >> 8);
}
