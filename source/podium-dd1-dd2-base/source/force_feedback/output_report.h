#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_REPORT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_REPORT_H

#include <stdbool.h>
#include <stdint.h>

enum { FORCE_OUTPUT_REPORT_SIZE = 5 };

typedef struct {
    bool positive_direction;
    uint16_t primary_magnitude;
    uint16_t secondary_magnitude;
} ForceOutputReport;

void force_output_report_encode(const ForceOutputReport *report,
                                uint8_t output[FORCE_OUTPUT_REPORT_SIZE]);

#endif
