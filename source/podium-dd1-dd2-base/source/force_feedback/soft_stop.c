#include "force_feedback/soft_stop.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Constants defining soft-stop onset, force span, and ramp timing. */
enum {
    FORCE_SOFT_STOP_ONSET_MARGIN = 1000,      /**< Position margin before force addition begins. */
    FORCE_SOFT_STOP_FULL_FORCE_SPAN = 0x19b1, /**< Position span reaching full force bias. */
    FORCE_SOFT_STOP_RAMP_INTERVAL_MS = 50, /**< Milliseconds between ramp percentage increases. */
    FORCE_SOFT_STOP_RAMP_RESET_DISTANCE = 494, /**< Inward boundary movement that resets ramp. */
    FORCE_SOFT_STOP_RAMP_MAXIMUM = 100,        /**< Maximum soft-stop ramp percentage. */
};

/** @brief Full-scale force bias used at either end-stop direction. */
static const int32_t force_soft_stop_target = INT32_C(0xffff);

void force_soft_stop_reset(ForceSoftStopState *state) { memset(state, 0, sizeof(*state)); }

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
    int32_t force_bias = 0;
    if (position > boundary) {
        int64_t distance = (int64_t)position - boundary;
        penetration = distance > FORCE_SOFT_STOP_FULL_FORCE_SPAN ? FORCE_SOFT_STOP_FULL_FORCE_SPAN
                                                                 : (int32_t)distance;
        force_bias = force_soft_stop_target;
    } else if (position < -boundary) {
        int64_t distance = -(int64_t)boundary - position;
        penetration = distance > FORCE_SOFT_STOP_FULL_FORCE_SPAN ? FORCE_SOFT_STOP_FULL_FORCE_SPAN
                                                                 : (int32_t)distance;
        force_bias = -force_soft_stop_target;
    }

    if (penetration != 0) {
        int32_t gradient = (accumulated_force + force_bias) / FORCE_SOFT_STOP_FULL_FORCE_SPAN;
        int64_t ramped_force =
            (int64_t)gradient * penetration * state->ramp_percent / FORCE_SOFT_STOP_RAMP_MAXIMUM;
        result.force += (int32_t)ramped_force;
    }
    result.outside_travel = position > config->travel_limit || position < -config->travel_limit;
    return result;
}
