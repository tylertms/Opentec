#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SOFT_STOP_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SOFT_STOP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t travel_limit;
} ForceSoftStopConfig;

typedef struct {
    int32_t previous_boundary;
    uint32_t next_ramp_ms;
    uint8_t ramp_percent;
} ForceSoftStopState;

typedef struct {
    int32_t force;
    bool outside_travel;
} ForceSoftStopResult;

void force_soft_stop_reset(ForceSoftStopState *state);
ForceSoftStopResult force_soft_stop_update(ForceSoftStopState *state,
                                           const ForceSoftStopConfig *config, int32_t position,
                                           int32_t accumulated_force, bool output_disabled,
                                           uint32_t now_ms);

#endif
