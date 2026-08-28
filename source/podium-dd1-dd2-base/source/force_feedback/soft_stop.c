#include "force_feedback/soft_stop.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    FORCE_SOFT_STOP_ONSET_MARGIN = 1000,
    FORCE_SOFT_STOP_FULL_FORCE_SPAN = 0x19b1,
    FORCE_SOFT_STOP_TARGET = 0xffff,
    FORCE_SOFT_STOP_RAMP_INTERVAL_MS = 50,
    FORCE_SOFT_STOP_RAMP_RESET_DISTANCE = 494,
    FORCE_SOFT_STOP_RAMP_MAXIMUM = 100,
};

void force_soft_stop_reset(ForceSoftStopState *state) { memset(state, 0, sizeof(*state)); }

/**
 * @brief Apply the wheel-range end stop to an accumulated force value.
 *
 * Starts force 1000 counts beyond the configured travel limit and blends the accumulator toward
 * positive or negative 65535 across 6577 counts. The blend ramps by one percent after each elapsed
 * 50-millisecond deadline. Reducing the boundary by at least 494 counts restarts that ramp. A
 * disabled secondary output suppresses both the force addition and the outside-travel state.
 *
 * @param[in,out] state End-stop ramp and previous-boundary state.
 * @param[in] config Current one-sided wheel travel limit.
 * @param[in] position Centered wheel position.
 * @param[in] accumulated_force Force accumulated before the end stop is applied.
 * @param[in] output_disabled True when the secondary force output is disabled.
 * @param[in] now_ms Current system time in milliseconds.
 * @return Updated accumulated force and the end-stop activity state.
 */
ForceSoftStopResult force_soft_stop_update(ForceSoftStopState *state,
                                           const ForceSoftStopConfig *config, int32_t position,
                                           int32_t accumulated_force, bool output_disabled,
                                           uint32_t now_ms) {
    int32_t boundary = config->travel_limit + FORCE_SOFT_STOP_ONSET_MARGIN;
    if (state->previous_boundary - boundary >= FORCE_SOFT_STOP_RAMP_RESET_DISTANCE) {
        state->ramp_percent = 0;
    }
    state->previous_boundary = boundary;

    if (state->ramp_percent < FORCE_SOFT_STOP_RAMP_MAXIMUM && now_ms > state->next_ramp_ms) {
        ++state->ramp_percent;
        state->next_ramp_ms = now_ms + FORCE_SOFT_STOP_RAMP_INTERVAL_MS;
    }

    ForceSoftStopResult result = {.force = accumulated_force};
    if (output_disabled) {
        return result;
    }

    int32_t penetration = 0;
    int32_t target = 0;
    if (position > boundary) {
        int64_t distance = (int64_t)position - boundary;
        penetration = distance > FORCE_SOFT_STOP_FULL_FORCE_SPAN ? FORCE_SOFT_STOP_FULL_FORCE_SPAN
                                                                 : (int32_t)distance;
        target = -FORCE_SOFT_STOP_TARGET;
    } else if (position < -boundary) {
        int64_t distance = -(int64_t)boundary - position;
        penetration = distance > FORCE_SOFT_STOP_FULL_FORCE_SPAN ? FORCE_SOFT_STOP_FULL_FORCE_SPAN
                                                                 : (int32_t)distance;
        target = FORCE_SOFT_STOP_TARGET;
    }

    if (penetration != 0) {
        int32_t gradient = (target - accumulated_force) / FORCE_SOFT_STOP_FULL_FORCE_SPAN;
        int64_t ramped_force =
            (int64_t)gradient * penetration * state->ramp_percent / FORCE_SOFT_STOP_RAMP_MAXIMUM;
        result.force += (int32_t)ramped_force;
    }
    result.outside_travel = position > config->travel_limit || position < -config->travel_limit;
    return result;
}
