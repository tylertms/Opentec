#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_H

#include <stdint.h>

typedef struct {
    uint16_t maximum_magnitude;
    uint16_t maximum_step;
} ForceOutputLimits;

typedef struct {
    int32_t value;
} ForceOutputState;

typedef struct {
    uint16_t magnitude;
    uint8_t direction;
    uint8_t active;
} ForceOutputCommand;

void force_output_reset(ForceOutputState *state);
ForceOutputCommand force_output_step(ForceOutputState *state, int32_t requested, uint8_t permitted,
                                     const ForceOutputLimits *limits);

#endif
