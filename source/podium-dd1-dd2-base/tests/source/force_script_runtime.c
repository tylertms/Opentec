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
    assert(system.scheduler.deadline == 0);
    assert(system.host_tick_snapshot == 0);
    assert(system.idle_tick_snapshot == 0);
}

static void test_resets_script_values_and_session_state(void) {
    ForceFeedbackScriptSystem system;
    force_feedback_script_runtime_init(&system);

    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT; index++) {
        system.values.variables[index] = UINT32_C(0x100) + index;
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT; index++) {
        system.values.motion[index] = UINT32_C(0x200) + index;
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT; index++) {
        system.values.axes[index] = UINT32_C(0x300) + index;
    }
    system.values.extended_rotation_range = 4;
    system.values.rotation_range_code = 5;
    system.values.active_slot = 1;
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        system.values.slots[slot].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
        for (uint8_t value = 0; value < 4; value++) {
            system.values.slots[slot].values[value] = UINT32_C(0x400) + slot * 4u + value;
        }
        system.values.slots[slot].average_rate = UINT32_C(0x500) + slot;
        system.values.slots[slot].delta_rate = UINT32_C(0x600) + slot;
        system.values.slots[slot].execution_count = UINT32_C(0x700) + slot;
        system.values.slots[slot].tick_snapshot = UINT32_C(0x800) + slot;
    }
    system.values.samples.values[15] = 16;
    system.store.data[0] = 17;
    system.store.slots[0] =
        (ForceFeedbackScriptStorageSlot){.offset = 0, .size = 1, .allocated = true};
    system.store.used = 1;
    system.inputs.status = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE;
    system.clock.ticks = 18;
    system.clock.slot_ticks[0] = 19;
    system.clock.motion_ticks = 20;
    system.clock.active_slot = 1;
    system.clock.script_executing = true;
    system.motion.tick_snapshot = 21;
    system.motion.previous_position = 22.0f;
    system.motion.previous_velocity = 23.0f;
    system.scheduler.deadline = 24;
    system.host_tick_snapshot = 25;
    system.idle_tick_snapshot = 26;
    system.mode = FORCE_FEEDBACK_RUNTIME_ACTIVE;

    force_feedback_script_runtime_reset(&system);

    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT; index++) {
        assert(system.values.variables[index] == 0);
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT; index++) {
        assert(system.values.motion[index] == 0);
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT; index++) {
        assert(system.values.axes[index] == 0);
    }
    assert(system.values.extended_rotation_range == 0);
    assert(system.values.rotation_range_code == 0);
    assert(system.values.active_slot == 0);
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        assert(system.values.slots[slot].state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);
        for (uint8_t value = 0; value < 4; value++) {
            assert(system.values.slots[slot].values[value] == 0);
        }
        assert(system.values.slots[slot].average_rate == 0);
        assert(system.values.slots[slot].delta_rate == 0);
        assert(system.values.slots[slot].execution_count == 0);
        assert(system.values.slots[slot].tick_snapshot == 0);
    }
    assert(system.values.samples.values[15] == UINT32_MAX);
    assert(system.store.used == 0);
    assert(!system.store.slots[0].allocated);
    assert(system.store.position_request_pending);
    assert(system.inputs.status == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION);
    assert(system.clock.ticks == 0);
    assert(system.clock.slot_ticks[0] == 0);
    assert(system.clock.motion_ticks == 20);
    assert(system.clock.active_slot == 0);
    assert(!system.clock.script_executing);
    assert(system.motion.tick_snapshot == 21);
    assert(system.motion.previous_position == 22.0f);
    assert(system.motion.previous_velocity == 23.0f);
    assert(system.scheduler.deadline == 24);
    assert(system.host_tick_snapshot == 25);
    assert(system.idle_tick_snapshot == 26);
    assert(system.mode == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);
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

static void test_routes_complete_script_packets(void) {
    ForceFeedbackScriptSystem system;
    force_feedback_script_runtime_init(&system);

    uint8_t samples[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {[0] = 0x0b};
    for (uint8_t record = 0; record < FORCE_FEEDBACK_SCRIPT_SAMPLE_UPDATE_COUNT; record++) {
        size_t offset = 4u + (size_t)record * 6u;
        samples[offset] = UINT8_MAX;
        samples[offset + 1] = UINT8_MAX;
    }
    samples[4] = 3;
    samples[5] = 0;
    samples[6] = 0x78;
    samples[7] = 0x56;
    samples[8] = 0x34;
    samples[9] = 0x12;
    assert(force_feedback_script_runtime_apply_packet(&system, samples, sizeof(samples)));
    assert(system.values.samples.values[3] == UINT32_C(0x12345678));

    uint8_t control[13] = {[0] = 0x0c, [12] = FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT};
    assert(force_feedback_script_runtime_apply_packet(&system, control, sizeof(control)));
    assert(system.mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT);

    uint8_t upload[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {
        [0] = 0x0d,
        [4] = 2,
        [5] = 1,
        [9] = 0xa5,
    };
    assert(force_feedback_script_runtime_apply_packet(&system, upload, sizeof(upload)));
    assert(system.store.slots[2].allocated);
    assert(system.store.data[0] == 0xa5);

    system.values.variables[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT_VARIABLE] = 100;
    uint8_t input[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {
        [0] = 0x0e,
        [4] = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE,
        [5] = 25,
    };
    input[9] = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED;
    input[18] = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED;
    input[27] = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED;
    assert(force_feedback_script_runtime_apply_packet(&system, input, sizeof(input)));
    assert(system.inputs.deadline == 125);

    uint8_t live_input[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {
        [0] = 0x0e,
        [4] = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE,
        [5] = 25,
        [9] = FORCE_FEEDBACK_SCRIPT_INPUT_POSITION,
        [10] = 0x78,
        [11] = 0x56,
        [12] = 0x34,
        [13] = 0x12,
        [18] = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED,
        [27] = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED,
    };
    assert(force_feedback_script_runtime_apply_packet(&system, live_input, sizeof(live_input)));
    assert(system.values.motion[4] == UINT32_C(0x12345678));
}

static void test_rejects_unknown_or_incomplete_script_packets(void) {
    ForceFeedbackScriptSystem system;
    force_feedback_script_runtime_init(&system);
    uint8_t unknown[] = {0x0f};

    assert(!force_feedback_script_runtime_apply_packet(&system, unknown, sizeof(unknown)));
    assert(!force_feedback_script_runtime_apply_packet(&system, unknown, 0));
    assert(!force_feedback_script_runtime_apply_packet(&system, NULL, 0));
    assert(!force_feedback_script_runtime_apply_packet(NULL, unknown, sizeof(unknown)));
    assert(system.mode == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);
    assert(system.store.used == 0);
}

int main(void) {
    test_initializes_complete_runtime();
    test_resets_script_values_and_session_state();
    test_applies_controls_and_compacts_storage();
    test_rejects_invalid_control_without_changes();
    test_routes_complete_script_packets();
    test_rejects_unknown_or_incomplete_script_packets();
    force_feedback_script_runtime_init(NULL);
    return 0;
}
