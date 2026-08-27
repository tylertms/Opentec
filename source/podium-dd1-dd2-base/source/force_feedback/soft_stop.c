#include "force_feedback/soft_stop.h"

#include <stdint.h>

static uint8_t deadline_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static int32_t force_boundary(const ForceSoftStopConfig *config) {
    return config->travel_limit + config->onset_margin;
}

static void update_ramp(ForceSoftStopState *state, const ForceSoftStopConfig *config,
                        uint32_t now_ms) {
    int32_t boundary = force_boundary(config);
    if (state->initialized == 0) {
        force_soft_stop_reset(state, config, now_ms);
    } else if (state->previous_limit - boundary > config->ramp_reset_distance) {
        state->ramp_percent = 0;
        state->next_ramp_ms = now_ms + config->ramp_step_interval_ms;
    }
    state->previous_limit = boundary;

    if (state->ramp_percent < 100 &&
        (config->ramp_step_interval_ms == 0 || deadline_reached(now_ms, state->next_ramp_ms))) {
        ++state->ramp_percent;
        state->next_ramp_ms = now_ms + config->ramp_step_interval_ms;
    }
}

static int32_t calculate_force(const ForceSoftStopConfig *config, int32_t position,
                               uint8_t ramp_percent) {
    if (config->full_force_span == 0 || config->maximum_force == 0 || ramp_percent == 0) {
        return 0;
    }

    int32_t boundary = force_boundary(config);
    int32_t penetration;
    int32_t direction;
    if (position > boundary) {
        penetration = position - boundary;
        direction = -1;
    } else if (position < -boundary) {
        penetration = -boundary - position;
        direction = 1;
    } else {
        return 0;
    }

    if (penetration > config->full_force_span) {
        penetration = config->full_force_span;
    }

    int64_t force = (int64_t)config->maximum_force * penetration * ramp_percent;
    return direction * (int32_t)(force / config->full_force_span / 100);
}

void force_soft_stop_reset(ForceSoftStopState *state, const ForceSoftStopConfig *config,
                           uint32_t now_ms) {
    state->previous_limit = force_boundary(config);
    state->next_ramp_ms = now_ms + config->ramp_step_interval_ms;
    state->ramp_percent = 0;
    state->initialized = 1;
}

ForceSoftStopResult force_soft_stop_update(ForceSoftStopState *state,
                                           const ForceSoftStopConfig *config, int32_t position,
                                           uint32_t now_ms) {
    update_ramp(state, config, now_ms);

    ForceSoftStopResult result = {
        .force = calculate_force(config, position, state->ramp_percent),
        .outside_travel = position > config->travel_limit || position < -config->travel_limit,
    };
    return result;
}
