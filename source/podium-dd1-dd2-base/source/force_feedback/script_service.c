#include "force_feedback/script_service.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Clock conversion constants used by script service metrics.
 *
 * Script clock counters advance at 10 kHz and are converted to seconds for the reported delta
 * metric.
 */
enum {
    FORCE_FEEDBACK_TICKS_PER_SECOND = 10000, /**< Script clock frequency in ticks per second. */
};

/** @brief Clock value above which the service resets its counters before execution. */
static const uint32_t FORCE_FEEDBACK_TICK_RESET_THRESHOLD = UINT32_C(0x337f9800);

/**
 * @brief Provides numeric and raw-bit views of a script service value.
 *
 * Metric calculations preserve floating-point results in the raw representation used by script
 * operands.
 */
typedef union {
    float number;  /**< Single-precision numeric view. */
    uint32_t bits; /**< Raw 32-bit representation. */
} ServiceValue;

/**
 * @brief Returns the bit representation of a script floating-point value.
 *
 * Preserves the 32-bit value without numeric conversion for storage in script-visible fields.
 *
 * @param[in] value Floating-point value to represent.
 * @return The unchanged 32-bit representation.
 */
static uint32_t float_bits(float value) { return (ServiceValue){.number = value}.bits; }

/**
 * @brief Updates one script slot's execution timing metrics.
 *
 * Increments the execution count, stores the snapshot divided by that count and scaled by the
 * 10 kHz clock in average_rate, and stores elapsed time since the slot's tick snapshot in seconds.
 *
 * @param[in,out] slot Script slot whose metrics are updated.
 * @param[in] current_ticks Current slot clock value after execution.
 */
static void update_metrics(ForceFeedbackScriptSlot *slot, uint32_t current_ticks) {
    slot->execution_count++;
    slot->average_rate = float_bits(((float)slot->tick_snapshot / (float)slot->execution_count) *
                                    (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
    slot->delta_rate = float_bits((float)(current_ticks - slot->tick_snapshot) /
                                  (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
}

/**
 * @brief Runs one script-service pass with optional execution tracking.
 *
 * @param[in,out] runtime Script runtime to advance.
 * @param[in] store Script storage containing the selected program.
 * @param[in,out] clock Script clock to advance.
 * @param[in] track_execution True to update execution metrics and state.
 * @return True when execution reaches the standard fault-report path; otherwise false.
 */
bool force_feedback_script_service_run_tracked(ForceFeedbackScriptRuntime *runtime,
                                               const ForceFeedbackScriptStore *store,
                                               ForceFeedbackScriptClock *clock,
                                               bool track_execution) {
    if (runtime == NULL || store == NULL || clock == NULL) {
        return false;
    }

    bool slot_faulted = false;
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
        clock->script_executing = track_execution;
        ForceFeedbackScriptExecutionStatus status =
            force_feedback_script_execute(runtime, &store->data[storage->offset], storage->size);
        slot_faulted = slot_faulted || status == FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT;
        update_metrics(slot, clock->slot_ticks[index]);
        clock->script_executing = false;
    }
    return slot_faulted;
}

bool force_feedback_script_service_run(ForceFeedbackScriptRuntime *runtime,
                                       const ForceFeedbackScriptStore *store,
                                       ForceFeedbackScriptClock *clock) {
    return force_feedback_script_service_run_tracked(runtime, store, clock, true);
}
