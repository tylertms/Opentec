#include "force_feedback/script_runtime.h"

#include <stddef.h>

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
    system->mode = FORCE_FEEDBACK_RUNTIME_POSITION_ONLY;
    system->store.position_request_pending = true;
}
