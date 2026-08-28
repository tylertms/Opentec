#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_STORE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_control.h"
#include "force_feedback/script_input.h"

enum {
    FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE = 2048,
    FORCE_FEEDBACK_SCRIPT_CHUNK_SIZE = 48,
};

typedef struct {
    uint16_t offset;
    uint16_t size;
    bool allocated;
} ForceFeedbackScriptStorageSlot;

typedef struct {
    uint8_t data[FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE];
    ForceFeedbackScriptStorageSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT];
    uint16_t used;
    bool position_request_pending;
} ForceFeedbackScriptStore;

void force_feedback_script_store_init(ForceFeedbackScriptStore *store);
bool force_feedback_script_store_upload(ForceFeedbackScriptStore *store,
                                        ForceFeedbackScriptSlot *runtime_slots,
                                        const uint8_t *packet, size_t length);
void force_feedback_script_store_compact(ForceFeedbackScriptStore *store,
                                         const ForceFeedbackScriptSlot *runtime_slots);
const uint8_t *force_feedback_script_store_data(const ForceFeedbackScriptStore *store, uint8_t slot,
                                                uint16_t *size);

#endif
