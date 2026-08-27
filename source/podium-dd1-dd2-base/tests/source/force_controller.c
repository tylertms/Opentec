#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/controller.h"

static ForceFeedbackConfig default_config(void) {
    ForceFeedbackConfig config = {
        .maximum_force = 10000,
        .maximum_step = 0,
        .filter_intensity = 100,
        .soft_stop =
            {
                .travel_limit = 5000,
                .onset_margin = 1000,
                .full_force_span = 1000,
                .maximum_force = 0,
                .ramp_step_interval_ms = 50,
                .ramp_reset_distance = 500,
            },
    };
    return config;
}

static void test_interlock(void) {
    ForceFeedbackController controller;
    ForceFeedbackConfig config = default_config();

    force_feedback_controller_init(&controller, &config, 0);
    ForceEffectBank *effects = force_feedback_controller_effects(&controller);
    assert(force_effect_bank_set_constant(effects, 0, 5000, FORCE_EFFECT_MAXIMUM_GAIN));
    assert(force_effect_bank_start(effects, 0));

    ForceOutputCommand output = force_feedback_controller_update(&controller, 0, 0, 0);
    assert(!output.active);

    force_feedback_controller_set_permitted(&controller, true);
    output = force_feedback_controller_update(&controller, 0, 0, 0);
    assert(output.active);
    assert(output.magnitude == 5000);
    assert(!output.negative);

    force_feedback_controller_set_permitted(&controller, false);
    output = force_feedback_controller_update(&controller, 0, 0, 0);
    assert(!output.active);
    assert(controller.output.value == 0);
}

static void test_effect_mixing_and_limit(void) {
    ForceFeedbackController controller;
    ForceFeedbackConfig config = default_config();

    force_feedback_controller_init(&controller, &config, 0);
    force_feedback_controller_set_permitted(&controller, true);
    ForceEffectBank *effects = force_feedback_controller_effects(&controller);
    assert(force_effect_bank_set_constant(effects, 0, 8000, FORCE_EFFECT_MAXIMUM_GAIN));
    assert(force_effect_bank_set_constant(effects, 1, 8000, FORCE_EFFECT_MAXIMUM_GAIN));
    assert(force_effect_bank_start(effects, 0));
    assert(force_effect_bank_start(effects, 1));

    ForceOutputCommand output = force_feedback_controller_update(&controller, 0, 0, 0);
    assert(output.magnitude == 10000);
    assert(!output.negative);
}

static void test_soft_stop(void) {
    ForceFeedbackController controller;
    ForceFeedbackConfig config = default_config();

    force_feedback_controller_init(&controller, &config, 0);
    force_feedback_controller_set_permitted(&controller, true);
    controller.soft_stop.ramp_percent = 100;

    ForceOutputCommand output = force_feedback_controller_update(&controller, 6500, 0, 0);
    assert(output.active);
    assert(output.magnitude == 5000);
    assert(output.negative);
}

static void test_slew_limit(void) {
    ForceFeedbackController controller;
    ForceFeedbackConfig config = default_config();
    config.maximum_step = 100;

    force_feedback_controller_init(&controller, &config, 0);
    force_feedback_controller_set_permitted(&controller, true);
    ForceEffectBank *effects = force_feedback_controller_effects(&controller);
    assert(force_effect_bank_set_constant(effects, 0, -1000, FORCE_EFFECT_MAXIMUM_GAIN));
    assert(force_effect_bank_start(effects, 0));

    ForceOutputCommand output = force_feedback_controller_update(&controller, 0, 0, 0);
    assert(output.magnitude == 100);
    assert(output.negative);
}

int main(void) {
    test_interlock();
    test_effect_mixing_and_limit();
    test_soft_stop();
    test_slew_limit();
    return 0;
}
