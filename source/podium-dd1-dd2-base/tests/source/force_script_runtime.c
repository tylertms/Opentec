#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_runtime.h"

static void test_initializes_complete_runtime(void) {
    ForceFeedbackScriptSystem system;
    memset(&system, 0xa5, sizeof(system));

    force_feedback_script_runtime_init(&system);

    assert(system.mode == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);
    assert(system.values.active_slot == 0);
    assert(system.values.variables[0] == 0);
    assert(system.values.motion[0] == 0);
    assert(system.values.axes[0] == 0);
    assert(system.values.slots[0].state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);
    assert(system.values.samples.values[0] == UINT32_MAX);
    assert(system.values.samples.values[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT - 1] == UINT32_MAX);
    assert(system.store.used == 0);
    assert(system.store.position_request_pending);
    assert(system.inputs.status == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION);
    assert(system.inputs.deadline == 0);
    assert(system.inputs.sample_count == 0);
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT; index++) {
        assert(system.inputs.slots[index].status == FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED);
        assert(system.inputs.slots[index].value == 0);
        assert(system.inputs.slots[index].duration == 0);
    }
    assert(system.clock.ticks == 0);
    assert(system.clock.slot_ticks[0] == 0);
    assert(system.clock.motion_ticks == 0);
    assert(system.clock.active_slot == 0);
    assert(!system.clock.script_executing);
}

int main(void) {
    test_initializes_complete_runtime();
    force_feedback_script_runtime_init(NULL);
    return 0;
}
