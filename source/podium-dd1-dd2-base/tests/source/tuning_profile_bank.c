#include <assert.h>
#include <stdint.h>

#include "profile/bank.h"

static void test_defaults(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    assert(bank.selected_slot == 0);
    assert(bank.active_slot == 0);
    for (uint8_t slot = 0; slot < TUNING_PROFILE_SLOT_COUNT; slot++) {
        assert(bank.slots[slot].rotation_degrees == 1080);
        assert(bank.slots[slot].force_feedback_strength == 35);
    }
}

static void test_selection_and_activation(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    assert(tuning_profile_bank_select(&bank, 4));
    assert(bank.selected_slot == 4);
    assert(bank.active_slot == 0);
    assert(tuning_profile_bank_selected(&bank) == &bank.slots[4]);

    tuning_profile_bank_activate_selected(&bank);
    assert(bank.active_slot == 4);
    assert(tuning_profile_bank_active(&bank) == &bank.slots[4]);

    assert(!tuning_profile_bank_select(&bank, TUNING_PROFILE_SLOT_COUNT));
    assert(bank.selected_slot == 4);
}

static void test_store_normalizes_profile(void) {
    TuningProfileBank bank;
    TuningProfile profile;
    tuning_profile_bank_defaults(&bank);
    tuning_profile_defaults(&profile);
    profile.rotation_degrees = 1087;
    profile.force_feedback_strength = UINT8_MAX;

    assert(tuning_profile_bank_store(&bank, 2, &profile));
    assert(bank.slots[2].rotation_degrees == 1080);
    assert(bank.slots[2].force_feedback_strength == 100);
    assert(profile.rotation_degrees == 1087);
    assert(!tuning_profile_bank_store(&bank, TUNING_PROFILE_SLOT_COUNT, &profile));
}

int main(void) {
    test_defaults();
    test_selection_and_activation();
    test_store_normalizes_profile();
    return 0;
}
