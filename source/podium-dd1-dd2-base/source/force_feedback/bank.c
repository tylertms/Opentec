#include "force_feedback/bank.h"

#include <stdint.h>

static ForceEffectSlot *get_slot(ForceEffectBank *bank, uint8_t slot) {
    return slot < FORCE_EFFECT_SLOT_COUNT ? &bank->slots[slot] : 0;
}

static int32_t evaluate_slot(const ForceEffectSlot *slot, int32_t position, int32_t velocity) {
    switch (slot->type) {
    case FORCE_EFFECT_CONSTANT:
        return force_effect_constant(slot->constant_magnitude, slot->constant_gain);
    case FORCE_EFFECT_SPRING:
        return force_effect_spring(&slot->spring, position);
    case FORCE_EFFECT_DAMPER:
        return force_effect_damper(&slot->damper, velocity);
    case FORCE_EFFECT_NONE:
        return 0;
    }
    return 0;
}

void force_effect_bank_reset(ForceEffectBank *bank, uint16_t maximum_force) {
    bank->maximum_force = maximum_force;
    for (uint8_t slot = 0; slot < FORCE_EFFECT_SLOT_COUNT; ++slot) {
        bank->slots[slot].type = FORCE_EFFECT_NONE;
        bank->slots[slot].active = 0;
    }
}

uint8_t force_effect_bank_set_constant(ForceEffectBank *bank, uint8_t slot, int32_t magnitude,
                                       uint16_t gain) {
    ForceEffectSlot *effect = get_slot(bank, slot);
    if (effect == 0) {
        return 0;
    }

    effect->type = FORCE_EFFECT_CONSTANT;
    effect->constant_magnitude = magnitude;
    effect->constant_gain = gain;
    return 1;
}

uint8_t force_effect_bank_set_spring(ForceEffectBank *bank, uint8_t slot,
                                     const ForceSpringEffect *spring) {
    ForceEffectSlot *effect = get_slot(bank, slot);
    if (effect == 0) {
        return 0;
    }

    effect->type = FORCE_EFFECT_SPRING;
    effect->spring = *spring;
    return 1;
}

uint8_t force_effect_bank_set_damper(ForceEffectBank *bank, uint8_t slot,
                                     const ForceDamperEffect *damper) {
    ForceEffectSlot *effect = get_slot(bank, slot);
    if (effect == 0) {
        return 0;
    }

    effect->type = FORCE_EFFECT_DAMPER;
    effect->damper = *damper;
    return 1;
}

uint8_t force_effect_bank_start(ForceEffectBank *bank, uint8_t slot) {
    ForceEffectSlot *effect = get_slot(bank, slot);
    if (effect == 0 || effect->type == FORCE_EFFECT_NONE) {
        return 0;
    }

    effect->active = 1;
    return 1;
}

uint8_t force_effect_bank_stop(ForceEffectBank *bank, uint8_t slot) {
    ForceEffectSlot *effect = get_slot(bank, slot);
    if (effect == 0) {
        return 0;
    }

    effect->active = 0;
    return 1;
}

uint8_t force_effect_bank_clear(ForceEffectBank *bank, uint8_t slot) {
    ForceEffectSlot *effect = get_slot(bank, slot);
    if (effect == 0) {
        return 0;
    }

    effect->type = FORCE_EFFECT_NONE;
    effect->active = 0;
    return 1;
}

int32_t force_effect_bank_evaluate(const ForceEffectBank *bank, int32_t position,
                                   int32_t velocity) {
    int32_t output = 0;
    for (uint8_t slot = 0; slot < FORCE_EFFECT_SLOT_COUNT; ++slot) {
        if (bank->slots[slot].active != 0) {
            output = force_effect_mix(output, evaluate_slot(&bank->slots[slot], position, velocity),
                                      bank->maximum_force);
        }
    }
    return output;
}
