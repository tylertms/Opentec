#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_control.h"

static void test_decodes_slot_commands_and_runtime_mode(void) {
    uint8_t packet[63] = {
        [0] = 0x0c, [4] = 0x01, [5] = 0x23,  [6] = 0x40,  [7] = 0x12,
        [8] = 0x34, [9] = 0x01, [10] = 0x23, [11] = 0x4f, [12] = FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT,
    };
    static const uint8_t expected[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {
        0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 15,
    };
    ForceFeedbackScriptControlResult result =
        force_feedback_script_control_decode(packet, sizeof(packet));

    assert(result.valid);
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        assert(result.value.slots[slot] == expected[slot]);
    }
    assert(result.value.runtime_mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT);
}

static void test_rejects_incomplete_commands(void) {
    uint8_t packet[13] = {[0] = 0x0c};

    assert(!force_feedback_script_control_decode(packet, 12).valid);
    packet[0] = 0x0b;
    assert(!force_feedback_script_control_decode(packet, sizeof(packet)).valid);
    assert(!force_feedback_script_control_decode(NULL, sizeof(packet)).valid);
}

static void test_encodes_slot_status_and_preserves_response_prefix(void) {
    ForceFeedbackScriptStatus status = {
        .slots =
            {
                FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY,
                FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE,
                FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE,
                FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED,
                FORCE_FEEDBACK_SCRIPT_SLOT_FAULT,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                13,
                14,
                15,
            },
        .runtime_mode = FORCE_FEEDBACK_RUNTIME_POSITION_ONLY,
    };
    uint8_t response[22] = {0xa5, 0xa5, 0xa5, 0xa5, 0xa5};

    assert(force_feedback_script_status_encode(&status, response, sizeof(response)));
    for (uint8_t index = 0; index < 5; index++) {
        assert(response[index] == 0xa5);
    }
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        uint8_t expected =
            slot == 4 ? FORCE_FEEDBACK_SCRIPT_SLOT_SERIALIZED_FAULT : status.slots[slot];
        assert(response[5 + slot] == expected);
    }
    assert(response[21] == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);

    assert(!force_feedback_script_status_encode(&status, response, 21));
    assert(!force_feedback_script_status_encode(NULL, response, sizeof(response)));
    assert(!force_feedback_script_status_encode(&status, NULL, sizeof(response)));
}

static void test_applies_slot_lifecycle(void) {
    ForceFeedbackScriptSlot slot = {
        .state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE,
        .values = {1, 2, 3, 4},
        .average_rate = 5,
        .delta_rate = 6,
        .execution_count = 7,
        .tick_snapshot = 8,
    };

    assert(force_feedback_script_slot_apply(&slot, FORCE_FEEDBACK_SCRIPT_SLOT_START));
    assert(slot.state == FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE);
    for (uint8_t index = 0; index < 4; ++index) {
        assert(slot.values[index] == 0);
    }
    assert(slot.average_rate == 0);
    assert(slot.delta_rate == 0);
    assert(slot.execution_count == 0);
    assert(slot.tick_snapshot == 0);

    assert(force_feedback_script_slot_apply(&slot, FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE));
    assert(slot.state == FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED);
    assert(force_feedback_script_slot_apply(&slot, FORCE_FEEDBACK_SCRIPT_SLOT_RESUME));
    assert(slot.state == FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE);
    assert(force_feedback_script_slot_apply(&slot, FORCE_FEEDBACK_SCRIPT_SLOT_STOP));
    assert(slot.state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);

    slot.values[0] = 123;
    assert(force_feedback_script_slot_apply(&slot, FORCE_FEEDBACK_SCRIPT_SLOT_CLEAR));
    assert(slot.state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);
    assert(slot.values[0] == 123);
}

static void test_preserves_state_for_rejected_transitions(void) {
    static const struct {
        ForceFeedbackScriptSlotState state;
        ForceFeedbackScriptSlotCommand command;
    } cases[] = {
        {FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY, FORCE_FEEDBACK_SCRIPT_SLOT_START},
        {FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY, FORCE_FEEDBACK_SCRIPT_SLOT_STOP},
        {FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE, FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE},
        {FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE, FORCE_FEEDBACK_SCRIPT_SLOT_RESUME},
        {FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE, 5},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        ForceFeedbackScriptSlot slot = {
            .state = cases[index].state,
            .values = {1, 2, 3, 4},
            .average_rate = 5,
            .delta_rate = 6,
            .execution_count = 7,
            .tick_snapshot = 8,
        };
        ForceFeedbackScriptSlot before = slot;
        assert(!force_feedback_script_slot_apply(&slot, cases[index].command));
        assert(memcmp(&slot, &before, sizeof(slot)) == 0);
    }
    assert(!force_feedback_script_slot_apply(NULL, FORCE_FEEDBACK_SCRIPT_SLOT_START));
}

static void test_advances_script_clocks(void) {
    ForceFeedbackScriptClock clock = {
        .ticks = UINT32_MAX,
        .slot_ticks = {[6] = UINT32_MAX},
        .motion_ticks = UINT32_MAX,
        .active_slot = 6,
        .script_executing = true,
    };

    force_feedback_script_clock_tick(&clock, FORCE_FEEDBACK_RUNTIME_ACTIVE);
    assert(clock.ticks == 0);
    assert(clock.slot_ticks[6] == 0);
    assert(clock.motion_ticks == 0);

    force_feedback_script_clock_tick(&clock, FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT);
    assert(clock.ticks == 1);
    assert(clock.slot_ticks[6] == 1);
    assert(clock.motion_ticks == 1);

    force_feedback_script_clock_tick(&clock, FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);
    assert(clock.ticks == 1);
    assert(clock.slot_ticks[6] == 2);
    assert(clock.motion_ticks == 2);

    clock.script_executing = false;
    force_feedback_script_clock_tick(&clock, 3);
    assert(clock.ticks == 1);
    assert(clock.slot_ticks[6] == 2);
    assert(clock.motion_ticks == 3);

    clock.script_executing = true;
    clock.active_slot = FORCE_FEEDBACK_SCRIPT_SLOT_COUNT;
    force_feedback_script_clock_tick(&clock, FORCE_FEEDBACK_RUNTIME_ACTIVE);
    assert(clock.ticks == 2);
    assert(clock.motion_ticks == 4);
    force_feedback_script_clock_tick(NULL, FORCE_FEEDBACK_RUNTIME_ACTIVE);
}

int main(void) {
    test_decodes_slot_commands_and_runtime_mode();
    test_rejects_incomplete_commands();
    test_encodes_slot_status_and_preserves_response_prefix();
    test_applies_slot_lifecycle();
    test_preserves_state_for_rejected_transitions();
    test_advances_script_clocks();
    return 0;
}
