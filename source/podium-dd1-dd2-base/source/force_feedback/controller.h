#ifndef OPENTEC_BASE_FORCE_FEEDBACK_CONTROLLER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/bank.h"
#include "force_feedback/filter.h"
#include "force_feedback/output.h"
#include "force_feedback/soft_stop.h"

typedef struct {
    uint16_t maximum_force;
    uint16_t maximum_step;
    uint8_t filter_intensity;
    ForceSoftStopConfig soft_stop;
} ForceFeedbackConfig;

typedef struct {
    ForceEffectBank effects;
    ForceFilter filter;
    ForceSoftStopState soft_stop;
    ForceOutputState output;
    ForceOutputLimits output_limits;
    ForceFeedbackConfig config;
    bool permitted;
} ForceFeedbackController;

void force_feedback_controller_init(ForceFeedbackController *controller,
                                    const ForceFeedbackConfig *config, uint32_t now_ms);
void force_feedback_controller_set_permitted(ForceFeedbackController *controller, bool permitted);
ForceEffectBank *force_feedback_controller_effects(ForceFeedbackController *controller);
ForceOutputCommand force_feedback_controller_update(ForceFeedbackController *controller,
                                                    int32_t position, int32_t velocity,
                                                    uint32_t now_ms);

#endif
