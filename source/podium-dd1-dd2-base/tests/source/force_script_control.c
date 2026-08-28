#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_control.h"

static void test_decodes_slot_commands_and_runtime_mode(void) {
    uint8_t packet[63] = {
        [0] = 0x0c, [4] = 0x01, [5] = 0x23,  [6] = 0x40,  [7] = 0x12,
        [8] = 0x34, [9] = 0x01, [10] = 0x23, [11] = 0x4f, [12] = FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT,
    };
    static const uint8_t expected[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {
        0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 15,
    };
    ForceFeedbackScriptControl control;

    assert(force_feedback_script_control_decode(packet, sizeof(packet), &control));
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        assert(control.slots[slot] == expected[slot]);
    }
    assert(control.runtime_mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT);
}

static void test_rejects_incomplete_commands(void) {
    uint8_t packet[13] = {[0] = 0x0c};
    ForceFeedbackScriptControl control;

    assert(!force_feedback_script_control_decode(packet, 12, &control));
    packet[0] = 0x0b;
    assert(!force_feedback_script_control_decode(packet, sizeof(packet), &control));
    assert(!force_feedback_script_control_decode(NULL, sizeof(packet), &control));
    assert(!force_feedback_script_control_decode(packet, sizeof(packet), NULL));
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

static ForceFeedbackScriptSlotTransition transition(ForceFeedbackScriptSlotState state,
                                                    ForceFeedbackScriptSlotCommand command,
                                                    bool accepted) {
    ForceFeedbackScriptSlotTransition result;
    assert(force_feedback_script_slot_transition(state, command, &result) == accepted);
    return result;
}

static void test_applies_slot_lifecycle(void) {
    ForceFeedbackScriptSlotTransition result =
        transition(FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE, FORCE_FEEDBACK_SCRIPT_SLOT_CLEAR, true);
    assert(result.state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);
    assert(!result.reset_runtime);

    result =
        transition(FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE, FORCE_FEEDBACK_SCRIPT_SLOT_START, true);
    assert(result.state == FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE);
    assert(result.reset_runtime);

    result = transition(FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE, FORCE_FEEDBACK_SCRIPT_SLOT_STOP, true);
    assert(result.state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);
    assert(!result.reset_runtime);

    result = transition(FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE, FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE, true);
    assert(result.state == FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED);

    result = transition(FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED, FORCE_FEEDBACK_SCRIPT_SLOT_RESUME, true);
    assert(result.state == FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE);
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
        ForceFeedbackScriptSlotTransition result =
            transition(cases[index].state, cases[index].command, false);
        assert(result.state == cases[index].state);
        assert(!result.reset_runtime);
    }
    assert(!force_feedback_script_slot_transition(FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE,
                                                  FORCE_FEEDBACK_SCRIPT_SLOT_START, NULL));
}

int main(void) {
    test_decodes_slot_commands_and_runtime_mode();
    test_rejects_incomplete_commands();
    test_encodes_slot_status_and_preserves_response_prefix();
    test_applies_slot_lifecycle();
    test_preserves_state_for_rejected_transitions();
    return 0;
}
