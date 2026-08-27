#ifndef OPENTEC_BASE_PROFILE_PERSISTENCE_H
#define OPENTEC_BASE_PROFILE_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"

enum { TUNING_PROFILE_SAVE_DELAY_MS = 1000 };

typedef enum {
    TUNING_PROFILE_PERSISTENCE_IDLE,
    TUNING_PROFILE_PERSISTENCE_SAVED,
    TUNING_PROFILE_PERSISTENCE_RETRY,
} TuningProfilePersistenceResult;

typedef struct {
    uint32_t generation;
    uint32_t write_after_ms;
    uint8_t active_slot;
    bool has_record;
    bool dirty;
} TuningProfilePersistence;

bool tuning_profile_persistence_load(TuningProfilePersistence *persistence, TuningProfileBank *bank,
                                     uint32_t now_ms);
void tuning_profile_persistence_mark_dirty(TuningProfilePersistence *persistence, uint32_t now_ms);
TuningProfilePersistenceResult
tuning_profile_persistence_service(TuningProfilePersistence *persistence,
                                   const TuningProfileBank *bank, uint32_t now_ms);

#endif
