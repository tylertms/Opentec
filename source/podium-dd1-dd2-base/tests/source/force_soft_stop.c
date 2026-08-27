#include <assert.h>
#include <stdint.h>

#include "force_feedback/soft_stop.h"

static ForceSoftStopConfig default_config(void) {
    ForceSoftStopConfig config = {
        .travel_limit = 2000,
        .onset_margin = 1000,
        .full_force_span = 1000,
        .maximum_force = 60000,
        .ramp_step_interval_ms = 50,
        .ramp_reset_distance = 493,
    };
    return config;
}

static void test_travel_and_force_boundaries(void) {
    ForceSoftStopConfig config = default_config();
    ForceSoftStopState state;
    force_soft_stop_reset(&state, &config, 0);
    state.ramp_percent = 100;

    ForceSoftStopResult result = force_soft_stop_update(&state, &config, 2000, 0);
    assert(result.outside_travel == 0);
    assert(result.force == 0);

    result = force_soft_stop_update(&state, &config, 2500, 0);
    assert(result.outside_travel == 1);
    assert(result.force == 0);

    result = force_soft_stop_update(&state, &config, 3500, 0);
    assert(result.outside_travel == 1);
    assert(result.force == -30000);

    result = force_soft_stop_update(&state, &config, -3500, 0);
    assert(result.outside_travel == 1);
    assert(result.force == 30000);
}

static void test_force_saturation(void) {
    ForceSoftStopConfig config = default_config();
    ForceSoftStopState state;
    force_soft_stop_reset(&state, &config, 0);
    state.ramp_percent = 100;

    assert(force_soft_stop_update(&state, &config, INT32_MAX, 0).force == -60000);
    assert(force_soft_stop_update(&state, &config, INT32_MIN, 0).force == 60000);
}

static void test_ramp_timing(void) {
    ForceSoftStopConfig config = default_config();
    ForceSoftStopState state;
    force_soft_stop_reset(&state, &config, 100);

    assert(force_soft_stop_update(&state, &config, 4000, 149).force == 0);
    assert(force_soft_stop_update(&state, &config, 4000, 150).force == -600);
    assert(force_soft_stop_update(&state, &config, 4000, 199).force == -600);
    assert(force_soft_stop_update(&state, &config, 4000, 200).force == -1200);
}

static void test_reset_threshold_does_not_restart_ramp(void) {
    ForceSoftStopConfig config = default_config();
    ForceSoftStopState state;
    force_soft_stop_reset(&state, &config, 0);
    state.ramp_percent = 50;

    config.travel_limit -= config.ramp_reset_distance;
    force_soft_stop_update(&state, &config, 0, 10);
    assert(state.ramp_percent == 50);
}

static void test_large_limit_reduction_restarts_ramp(void) {
    ForceSoftStopConfig config = default_config();
    ForceSoftStopState state;
    force_soft_stop_reset(&state, &config, 0);
    state.ramp_percent = 50;

    config.travel_limit -= config.ramp_reset_distance + 1;
    force_soft_stop_update(&state, &config, 0, 20);
    assert(state.ramp_percent == 0);
    assert(state.next_ramp_ms == 70);
}

static void test_disabled_force(void) {
    ForceSoftStopConfig config = default_config();
    ForceSoftStopState state;
    force_soft_stop_reset(&state, &config, 0);
    state.ramp_percent = 100;

    config.maximum_force = 0;
    ForceSoftStopResult result = force_soft_stop_update(&state, &config, 4000, 0);
    assert(result.force == 0);
    assert(result.outside_travel == 1);

    config.maximum_force = 60000;
    config.full_force_span = 0;
    assert(force_soft_stop_update(&state, &config, 4000, 0).force == 0);
}

int main(void) {
    test_travel_and_force_boundaries();
    test_force_saturation();
    test_ramp_timing();
    test_reset_threshold_does_not_restart_ramp();
    test_large_limit_reduction_restarts_ramp();
    test_disabled_force();
    return 0;
}
