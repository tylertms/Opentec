#include "force_feedback/script_service.h"

#include <stddef.h>
#include <stdint.h>

enum {
    FORCE_FEEDBACK_TICKS_PER_SECOND = 10000,
};

static const uint32_t FORCE_FEEDBACK_TICK_RESET_THRESHOLD = UINT32_C(0x337f9800);

typedef union {
    float number;
    uint32_t bits;
} ServiceValue;

static uint32_t float_bits(float value) { return (ServiceValue){.number = value}.bits; }

static void update_metrics(ForceFeedbackScriptSlot *slot, uint32_t current_ticks) {
    slot->execution_count++;
    slot->average_rate = float_bits(((float)slot->tick_snapshot / (float)slot->execution_count) *
                                    (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
    slot->delta_rate = float_bits((float)(current_ticks - slot->tick_snapshot) /
                                  (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
}

/**
 * @brief Run every active force-feedback script slot once.
 *
 * Visits the 16 slots in ascending order. For each active slot, selects its stored byte sequence,
 * normalizes an aged timing counter, snapshots the counter, executes the script, and updates the
 * execution count and timing values. The clock identifies the slot while execution is in progress
 * so timer interrupts can charge elapsed ticks to the correct slot.
 *
 * @param[in,out] runtime Script operands, samples, outputs, axes, and per-slot runtime state.
 * @param[in] store Allocated byte sequences for the script slots.
 * @param[in,out] clock Per-slot timing counters and current execution markers.
 * @pre Every active runtime slot has a corresponding valid allocation in store.
 */
void force_feedback_script_service_run(ForceFeedbackScriptRuntime *runtime,
                                       const ForceFeedbackScriptStore *store,
                                       ForceFeedbackScriptClock *clock) {
    if (runtime == NULL || store == NULL || clock == NULL) {
        return;
    }

    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; index++) {
        ForceFeedbackScriptSlot *slot = &runtime->slots[index];
        if (slot->state != FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE) {
            continue;
        }
        runtime->active_slot = index;
        clock->active_slot = index;
        if (clock->slot_ticks[index] > FORCE_FEEDBACK_TICK_RESET_THRESHOLD) {
            clock->slot_ticks[index] = 0;
        }
        slot->tick_snapshot = clock->slot_ticks[index];

        const ForceFeedbackScriptStorageSlot *storage = &store->slots[index];
        clock->script_executing = true;
        force_feedback_script_execute(runtime, &store->data[storage->offset], storage->size);
        update_metrics(slot, clock->slot_ticks[index]);
        clock->script_executing = false;
    }
}
