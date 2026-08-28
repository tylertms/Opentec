#include "force_feedback/script_runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the complete force-feedback script runtime.
 *
 * Selects position-only mode, clears script values, slot states, storage, input timing, and clock
 * counters, marks all 512 samples and three live-input slots unused, and requests an initial live
 * position update. Operation dispatch is static in this implementation and needs no handler-table
 * initialization.
 *
 * @param[out] system Script values, storage, inputs, timing state, and runtime mode to initialize.
 */
void force_feedback_script_runtime_init(ForceFeedbackScriptSystem *system) {
    if (system == NULL) {
        return;
    }

    system->values = (ForceFeedbackScriptRuntime){0};
    force_feedback_script_samples_init(&system->values.samples);
    force_feedback_script_store_init(&system->store);
    force_feedback_script_inputs_init(&system->inputs);
    system->clock = (ForceFeedbackScriptClock){0};
    system->motion = (ForceFeedbackScriptMotionState){0};
    system->mode = FORCE_FEEDBACK_RUNTIME_POSITION_ONLY;
    system->store.position_request_pending = true;
}

/**
 * @brief Apply a force-feedback script control packet to the runtime.
 *
 * Applies the packed commands to all 16 slots in ascending order, replaces the runtime mode with
 * packet byte 12, and compacts storage released by clear commands. Start resets a retained slot's
 * values and metrics through the shared slot lifecycle implementation.
 *
 * @param[in,out] system Script runtime whose slots, mode, and storage are updated.
 * @param[in] packet Feature-command packet beginning with script-control opcode 0x0C.
 * @param[in] length Number of available packet bytes.
 * @return True when the complete packet is decoded and applied.
 */
bool force_feedback_script_runtime_apply_control(ForceFeedbackScriptSystem *system,
                                                 const uint8_t *packet, size_t length) {
    if (system == NULL) {
        return false;
    }

    ForceFeedbackScriptControlResult result = force_feedback_script_control_decode(packet, length);
    if (!result.valid) {
        return false;
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; index++) {
        force_feedback_script_slot_apply(&system->values.slots[index], result.value.slots[index]);
    }
    system->mode = result.value.runtime_mode;
    force_feedback_script_store_compact(&system->store, system->values.slots);
    return true;
}
