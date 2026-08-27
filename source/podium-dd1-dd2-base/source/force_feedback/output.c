#include "force_feedback/output.h"

#include <stdint.h>

static int32_t clamp_force(int32_t requested, uint16_t maximum) {
    if (requested < -(int32_t)maximum) {
        return -(int32_t)maximum;
    }
    if (requested > maximum) {
        return maximum;
    }
    return requested;
}

static int32_t step_toward(int32_t current, int32_t target, uint16_t maximum_step) {
    if (maximum_step == 0) {
        return target;
    }

    int32_t difference = target - current;
    if (difference < -(int32_t)maximum_step) {
        return current - maximum_step;
    }
    if (difference > maximum_step) {
        return current + maximum_step;
    }
    return target;
}

static ForceOutputCommand make_command(int32_t value) {
    ForceOutputCommand command = {
        .magnitude = value < 0 ? (uint16_t)-value : (uint16_t)value,
        .negative = value < 0,
        .active = value != 0,
    };
    return command;
}

void force_output_reset(ForceOutputState *state) { state->value = 0; }

ForceOutputCommand force_output_step(ForceOutputState *state, int32_t requested, uint8_t permitted,
                                     const ForceOutputLimits *limits) {
    if (permitted == 0 || limits->maximum_magnitude == 0) {
        state->value = 0;
        return make_command(0);
    }

    int32_t target = clamp_force(requested, limits->maximum_magnitude);
    state->value = step_toward(state->value, target, limits->maximum_step);
    return make_command(state->value);
}
