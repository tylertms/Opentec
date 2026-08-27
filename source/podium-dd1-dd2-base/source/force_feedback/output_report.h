#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_REPORT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_REPORT_H

#include <stdint.h>

#include "force_feedback/output.h"

enum { FORCE_OUTPUT_REPORT_SIZE = 5 };

void force_output_report_encode(const ForceOutputCommand *command, uint16_t secondary_magnitude,
                                uint8_t output[FORCE_OUTPUT_REPORT_SIZE]);

#endif
