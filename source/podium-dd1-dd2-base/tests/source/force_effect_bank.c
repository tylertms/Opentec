#include <assert.h>
#include <stdint.h>

#include "force_feedback/bank.h"

static void test_slot_lifecycle(void) {
    ForceEffectBank bank;
    force_effect_bank_reset(&bank, UINT16_MAX);

    assert(force_effect_bank_start(&bank, 0) == 0);
    assert(force_effect_bank_set_constant(&bank, 0, 30000, 10000) == 1);
    assert(force_effect_bank_evaluate(&bank, 0, 0) == 0);
    assert(force_effect_bank_start(&bank, 0) == 1);
    assert(force_effect_bank_evaluate(&bank, 0, 0) == 30000);
    assert(force_effect_bank_stop(&bank, 0) == 1);
    assert(force_effect_bank_evaluate(&bank, 0, 0) == 0);
    assert(force_effect_bank_start(&bank, 0) == 1);
    assert(force_effect_bank_clear(&bank, 0) == 1);
    assert(force_effect_bank_evaluate(&bank, 0, 0) == 0);
}

static void test_effect_mixing(void) {
    ForceEffectBank bank;
    force_effect_bank_reset(&bank, 50000);

    const ForceSpringEffect spring = {
        .center = 0,
        .deadband = 0,
        .saturation_distance = 1000,
        .maximum_force = 40000,
        .gain = 10000,
    };
    const ForceDamperEffect damper = {
        .saturation_velocity = 1000,
        .maximum_force = 20000,
        .gain = 10000,
    };

    assert(force_effect_bank_set_constant(&bank, 0, 10000, 10000) == 1);
    assert(force_effect_bank_set_spring(&bank, 1, &spring) == 1);
    assert(force_effect_bank_set_damper(&bank, 2, &damper) == 1);
    assert(force_effect_bank_start(&bank, 0) == 1);
    assert(force_effect_bank_start(&bank, 1) == 1);
    assert(force_effect_bank_start(&bank, 2) == 1);

    assert(force_effect_bank_evaluate(&bank, 500, 500) == -20000);
    assert(force_effect_bank_evaluate(&bank, 1000, 1000) == -50000);
    assert(force_effect_bank_evaluate(&bank, -1000, -1000) == 50000);
}

static void test_reconfigure_active_slot(void) {
    ForceEffectBank bank;
    force_effect_bank_reset(&bank, UINT16_MAX);
    assert(force_effect_bank_set_constant(&bank, 4, 100, 10000) == 1);
    assert(force_effect_bank_start(&bank, 4) == 1);

    const ForceDamperEffect damper = {
        .saturation_velocity = 100,
        .maximum_force = 1000,
        .gain = 10000,
    };
    assert(force_effect_bank_set_damper(&bank, 4, &damper) == 1);
    assert(force_effect_bank_evaluate(&bank, 0, 50) == -500);
}

static void test_invalid_slots(void) {
    ForceEffectBank bank;
    force_effect_bank_reset(&bank, UINT16_MAX);
    const ForceSpringEffect spring = {0};
    const ForceDamperEffect damper = {0};

    assert(force_effect_bank_set_constant(&bank, FORCE_EFFECT_SLOT_COUNT, 0, 0) == 0);
    assert(force_effect_bank_set_spring(&bank, FORCE_EFFECT_SLOT_COUNT, &spring) == 0);
    assert(force_effect_bank_set_damper(&bank, FORCE_EFFECT_SLOT_COUNT, &damper) == 0);
    assert(force_effect_bank_start(&bank, FORCE_EFFECT_SLOT_COUNT) == 0);
    assert(force_effect_bank_stop(&bank, FORCE_EFFECT_SLOT_COUNT) == 0);
    assert(force_effect_bank_clear(&bank, FORCE_EFFECT_SLOT_COUNT) == 0);
}

int main(void) {
    test_slot_lifecycle();
    test_effect_mixing();
    test_reconfigure_active_slot();
    test_invalid_slots();
    return 0;
}
