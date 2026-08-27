#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SOFT_STOP_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SOFT_STOP_H

#include <stdint.h>

typedef struct {
    int32_t travel_limit;
    uint16_t onset_margin;
    uint16_t full_force_span;
    uint16_t maximum_force;
    uint16_t ramp_step_interval_ms;
    uint16_t ramp_reset_distance;
} ForceSoftStopConfig;

typedef struct {
    int32_t previous_limit;
    uint32_t next_ramp_ms;
    uint8_t ramp_percent;
    uint8_t initialized;
} ForceSoftStopState;

typedef struct {
    int32_t force;
    uint8_t outside_travel;
} ForceSoftStopResult;

void force_soft_stop_reset(ForceSoftStopState *state, const ForceSoftStopConfig *config,
                           uint32_t now_ms);
ForceSoftStopResult force_soft_stop_update(ForceSoftStopState *state,
                                           const ForceSoftStopConfig *config, int32_t position,
                                           uint32_t now_ms);

#endif
