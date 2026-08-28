#ifndef OPENTEC_BASE_PROFILE_BANK_H
#define OPENTEC_BASE_PROFILE_BANK_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"

enum { TUNING_PROFILE_SLOT_COUNT = 6 };

typedef struct {
    TuningProfile slots[TUNING_PROFILE_SLOT_COUNT];
    uint8_t selected_slot;
    uint8_t active_slot;
    bool standard_mode_enabled;
} TuningProfileBank;

void tuning_profile_bank_defaults(TuningProfileBank *bank);
bool tuning_profile_bank_select(TuningProfileBank *bank, uint8_t slot);
void tuning_profile_bank_activate_selected(TuningProfileBank *bank);
bool tuning_profile_bank_store(TuningProfileBank *bank, uint8_t slot, const TuningProfile *profile);
const TuningProfile *tuning_profile_bank_selected(const TuningProfileBank *bank);
const TuningProfile *tuning_profile_bank_active(const TuningProfileBank *bank);

#endif
