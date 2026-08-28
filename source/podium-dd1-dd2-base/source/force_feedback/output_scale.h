#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_SCALE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_SCALE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"

typedef struct {
    uint8_t available_percent;
    uint8_t tuning_strength_percent;
    uint8_t output_strength_percent;
    bool secondary_output_disabled;
} ForceOutputScale;

void force_output_scale_apply(int32_t force, int32_t secondary_magnitude,
                              const ForceOutputScale *scale, ForceOutputReport *report);

#endif
