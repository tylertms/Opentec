#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OUTPUT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OUTPUT_H

#include <stdint.h>

int32_t force_feedback_script_output_request(uint32_t motion, int8_t strength,
                                             uint8_t automatic_strength);
int32_t force_feedback_script_output_ramp(int32_t filtered, uint8_t ramp_percent);

#endif
