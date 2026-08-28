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
    assert(system.motion.tick_snapshot == 0);
    assert(system.motion.previous_position == 0.0f);
    assert(system.motion.previous_velocity == 0.0f);
}

static void test_applies_controls_and_compacts_storage(void) {
    ForceFeedbackScriptSystem system;
    force_feedback_script_runtime_init(&system);
    system.values.slots[0].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    system.values.slots[0].values[0] = 7;
    system.values.slots[1].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    system.values.slots[2].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    system.store.data[0] = 10;
    system.store.data[1] = 20;
    system.store.data[2] = 30;
    system.store.slots[0] =
        (ForceFeedbackScriptStorageSlot){.offset = 0, .size = 1, .allocated = true};
    system.store.slots[1] =
        (ForceFeedbackScriptStorageSlot){.offset = 1, .size = 1, .allocated = true};
    system.store.slots[2] =
        (ForceFeedbackScriptStorageSlot){.offset = 2, .size = 1, .allocated = true};
    system.store.used = 3;

    uint8_t packet[13] = {
        [0] = 0x0c,
        [4] = 0x13,
        [12] = FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT,
    };
    assert(force_feedback_script_runtime_apply_control(&system, packet, sizeof(packet)));

    assert(system.values.slots[0].state == FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE);
    assert(system.values.slots[0].values[0] == 0);
    assert(system.values.slots[1].state == FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED);
    assert(system.values.slots[2].state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);
    assert(system.mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT);
    assert(system.store.used == 2);
    assert(system.store.data[0] == 10);
    assert(system.store.data[1] == 20);
    assert(system.store.slots[1].offset == 1);
    assert(!system.store.slots[2].allocated);
}

static void test_rejects_invalid_control_without_changes(void) {
    ForceFeedbackScriptSystem system;
    force_feedback_script_runtime_init(&system);
    system.values.slots[0].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    uint8_t packet[12] = {[0] = 0x0c};

    assert(!force_feedback_script_runtime_apply_control(&system, packet, sizeof(packet)));
    assert(system.values.slots[0].state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);
    assert(system.mode == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);
}

int main(void) {
    test_initializes_complete_runtime();
    test_applies_controls_and_compacts_storage();
    test_rejects_invalid_control_without_changes();
    force_feedback_script_runtime_init(NULL);
    return 0;
}
