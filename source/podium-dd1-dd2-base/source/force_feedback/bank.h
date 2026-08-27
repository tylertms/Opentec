#ifndef OPENTEC_BASE_FORCE_FEEDBACK_BANK_H
#define OPENTEC_BASE_FORCE_FEEDBACK_BANK_H

#include <stdint.h>

#include "force_feedback/effect.h"

enum { FORCE_EFFECT_SLOT_COUNT = 16 };

typedef enum {
    FORCE_EFFECT_NONE,
    FORCE_EFFECT_CONSTANT,
    FORCE_EFFECT_SPRING,
    FORCE_EFFECT_DAMPER,
} ForceEffectType;

typedef struct {
    ForceEffectType type;
    uint8_t active;
    int32_t constant_magnitude;
    uint16_t constant_gain;
    ForceSpringEffect spring;
    ForceDamperEffect damper;
} ForceEffectSlot;

typedef struct {
    ForceEffectSlot slots[FORCE_EFFECT_SLOT_COUNT];
    uint16_t maximum_force;
} ForceEffectBank;

void force_effect_bank_reset(ForceEffectBank *bank, uint16_t maximum_force);
uint8_t force_effect_bank_set_constant(ForceEffectBank *bank, uint8_t slot, int32_t magnitude,
                                       uint16_t gain);
uint8_t force_effect_bank_set_spring(ForceEffectBank *bank, uint8_t slot,
                                     const ForceSpringEffect *effect);
uint8_t force_effect_bank_set_damper(ForceEffectBank *bank, uint8_t slot,
                                     const ForceDamperEffect *effect);
uint8_t force_effect_bank_start(ForceEffectBank *bank, uint8_t slot);
uint8_t force_effect_bank_stop(ForceEffectBank *bank, uint8_t slot);
uint8_t force_effect_bank_clear(ForceEffectBank *bank, uint8_t slot);
int32_t force_effect_bank_evaluate(const ForceEffectBank *bank, int32_t position, int32_t velocity);

#endif
