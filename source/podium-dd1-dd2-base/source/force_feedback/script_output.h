#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OUTPUT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/filter.h"
#include "force_feedback/output_report.h"
#include "force_feedback/soft_stop.h"

typedef struct {
    ForceFilter filter;
    ForceSoftStopState soft_stop;
} ForceFeedbackScriptOutputState;

typedef struct {
    ForceSoftStopConfig soft_stop;
    uint16_t available_percent;
    int32_t output_strength_percent;
    uint8_t automatic_strength;
    uint8_t ramp_percent;
    uint8_t smoothing_intensity;
    int8_t tuning_strength;
    bool secondary_output_disabled;
} ForceFeedbackScriptOutputConfig;

int32_t force_feedback_script_output_request(uint32_t motion, int8_t strength,
                                             uint8_t automatic_strength);
int32_t force_feedback_script_output_ramp(int32_t filtered, uint8_t ramp_percent);
void force_feedback_script_output_init(ForceFeedbackScriptOutputState *state);
bool force_feedback_script_output_apply(ForceFeedbackScriptOutputState *state, uint32_t motion,
                                        int32_t position, uint32_t now_ms,
                                        const ForceFeedbackScriptOutputConfig *config,
                                        ForceOutputReport *report);

#endif
