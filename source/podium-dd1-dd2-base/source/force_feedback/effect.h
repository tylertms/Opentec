#ifndef OPENTEC_BASE_FORCE_FEEDBACK_EFFECT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_EFFECT_H

#include <stdint.h>

enum { FORCE_EFFECT_MAXIMUM_GAIN = 10000 };

typedef struct {
    int32_t center;
    uint32_t deadband;
    uint32_t saturation_distance;
    uint16_t maximum_force;
    uint16_t gain;
} ForceSpringEffect;

typedef struct {
    uint32_t saturation_velocity;
    uint16_t maximum_force;
    uint16_t gain;
} ForceDamperEffect;

int32_t force_effect_constant(int32_t magnitude, uint16_t gain);
int32_t force_effect_spring(const ForceSpringEffect *effect, int32_t position);
int32_t force_effect_damper(const ForceDamperEffect *effect, int32_t velocity);
int32_t force_effect_mix(int32_t accumulated, int32_t contribution, uint16_t maximum_force);

#endif
