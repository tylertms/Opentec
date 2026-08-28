#include "force_feedback/controller.h"

#include <stdbool.h>
#include <stdint.h>

void force_feedback_controller_init(ForceFeedbackController *controller,
                                    const ForceFeedbackConfig *config) {
    controller->config = *config;
    controller->output_limits = (ForceOutputLimits){
        .maximum_magnitude = config->maximum_force,
        .maximum_step = config->maximum_step,
    };
    controller->permitted = false;
    force_effect_bank_reset(&controller->effects, config->maximum_force);
    controller->filter = (ForceFilter){0};
    force_filter_configure(&controller->filter, config->filter_intensity);
    force_soft_stop_reset(&controller->soft_stop);
    force_output_reset(&controller->output);
}

void force_feedback_controller_set_permitted(ForceFeedbackController *controller, bool permitted) {
    controller->permitted = permitted;
    if (!permitted) {
        force_output_reset(&controller->output);
    }
}

ForceEffectBank *force_feedback_controller_effects(ForceFeedbackController *controller) {
    return &controller->effects;
}

ForceOutputCommand force_feedback_controller_update(ForceFeedbackController *controller,
                                                    int32_t position, int32_t velocity,
                                                    uint32_t now_ms) {
    int32_t force = force_effect_bank_evaluate(&controller->effects, position, velocity);
    ForceSoftStopResult soft_stop = force_soft_stop_update(
        &controller->soft_stop, &controller->config.soft_stop, position, force, false, now_ms);
    force = soft_stop.force;
    force = force_filter_update(&controller->filter, force, now_ms);

    return force_output_step(&controller->output, force, controller->permitted,
                             &controller->output_limits);
}
