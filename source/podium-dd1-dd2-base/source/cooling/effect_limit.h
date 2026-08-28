#ifndef OPENTEC_BASE_COOLING_EFFECT_LIMIT_H
#define OPENTEC_BASE_COOLING_EFFECT_LIMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "cooling/controller.h"

typedef struct {
    uint8_t force;
    uint8_t spring;
    uint8_t damper;
} CoolingEffectStrengths;

typedef enum {
    COOLING_EFFECT_LIMIT_INACTIVE,
    COOLING_EFFECT_LIMIT_STANDARD,
    COOLING_EFFECT_LIMIT_AUXILIARY,
} CoolingEffectLimitPhase;

typedef struct {
    CoolingEffectLimitPhase phase;
    CoolingEffectStrengths snapshot;
    bool active;
} CoolingEffectLimit;

void cooling_effect_limit_init(CoolingEffectLimit *limit);
void cooling_effect_limit_update(CoolingEffectLimit *limit, CoolingEffectStrengths *strengths,
                                 const CoolingController *controller, float motor_temperature_c,
                                 bool auxiliary_active, uint32_t now_ms);

#endif
