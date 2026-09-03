#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/soft_stop.h"

static const ForceSoftStopConfig config = {.travel_limit = 2000};

static void test_travel_and_force_boundaries(void) {
    ForceSoftStopState state;
    force_soft_stop_reset(&state);
    state.ramp_percent = 100;

    ForceSoftStopResult result = force_soft_stop_update(&state, &config, 2000, 0, false, 0);
    assert(!result.outside_travel);
    assert(result.force == 0);

    result = force_soft_stop_update(&state, &config, 2500, 0, false, 0);
    assert(result.outside_travel);
    assert(result.force == 0);

    result = force_soft_stop_update(&state, &config, 3500, 0, false, 0);
    assert(result.outside_travel);
    assert(result.force == -4500);

    result = force_soft_stop_update(&state, &config, -3500, 0, false, 0);
    assert(result.outside_travel);
    assert(result.force == 4500);
}

static void test_force_saturation(void) {
    ForceSoftStopState state;
    force_soft_stop_reset(&state);
    state.ramp_percent = 100;

    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0, false, 0).force == -59193);
    assert(force_soft_stop_update(&state, &config, INT32_MIN, 0, false, 0).force == 59193);
}

static void test_existing_force_blend(void) {
    ForceSoftStopState state;
    force_soft_stop_reset(&state);
    state.ramp_percent = 100;

    int32_t positive = force_soft_stop_update(&state, &config, INT32_MAX, 20000, false, 0).force;
    int32_t negative = force_soft_stop_update(&state, &config, INT32_MIN, -20000, false, 0).force;
    assert(positive == -19462);
    assert(negative == 19462);
}

static void test_ramp_timing(void) {
    ForceSoftStopState state;
    force_soft_stop_reset(&state);

    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0, false, 0).force == 0);
    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0, false, 1).force == -591);
    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0, false, 50).force == -591);
    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0, false, 51).force == -591);
    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0, false, 52).force == -1183);
}

static void test_range_reduction_restarts_ramp(void) {
    ForceSoftStopState state;
    force_soft_stop_reset(&state);
    state.ramp_percent = 50;
    state.next_ramp_ms = 100;
    force_soft_stop_update(&state, &config, 0, 0, false, 0);

    ForceSoftStopConfig reduced = {.travel_limit = config.travel_limit - 493};
    force_soft_stop_update(&state, &reduced, 0, 0, false, 20);
    assert(state.ramp_percent == 50);

    reduced.travel_limit -= 1;
    force_soft_stop_update(&state, &reduced, 0, 0, false, 20);
    assert(state.ramp_percent == 50);

    reduced.travel_limit -= 494;
    force_soft_stop_update(&state, &reduced, 0, 0, false, 20);
    assert(state.ramp_percent == 0);
    assert(state.next_ramp_ms == 100);
}

static void test_disabled_output(void) {
    ForceSoftStopState state;
    force_soft_stop_reset(&state);
    state.ramp_percent = 100;

    ForceSoftStopResult result = force_soft_stop_update(&state, &config, INT32_MAX, 1234, true, 0);
    assert(result.force == 1234);
    assert(!result.outside_travel);
}

int main(void) {
    test_travel_and_force_boundaries();
    test_force_saturation();
    test_existing_force_blend();
    test_ramp_timing();
    test_range_reduction_restarts_ramp();
    test_disabled_output();
    return 0;
}
