#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_report.h"

static uint32_t decode_value(const uint8_t input[4]) {
    return (uint32_t)input[0] | (uint32_t)input[1] << 8u | (uint32_t)input[2] << 16u |
           (uint32_t)input[3] << 24u;
}

static void test_encodes_complete_slot_status_response(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    static const ForceFeedbackScriptSlotState states[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {
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
    };
    uint8_t sequence = 1;
    uint8_t response[FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE] = {0};
    static const uint8_t expected_envelope[] = {0x25, 0, 2, 0x12, 6};

    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        runtime.slots[slot].state = states[slot];
    }
    assert(force_feedback_script_status_report_encode(
        &runtime, FORCE_FEEDBACK_RUNTIME_POSITION_ONLY, &sequence, response, sizeof(response)));
    assert(sequence == 2);
    assert(memcmp(response, expected_envelope, sizeof(expected_envelope)) == 0);
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot++) {
        uint8_t expected = slot == 4 ? FORCE_FEEDBACK_SCRIPT_SLOT_SERIALIZED_FAULT : states[slot];
        assert(response[5 + slot] == expected);
    }
    assert(response[21] == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY);

    sequence = UINT8_MAX;
    assert(force_feedback_script_status_report_encode(&runtime, FORCE_FEEDBACK_RUNTIME_ACTIVE,
                                                      &sequence, response, sizeof(response)));
    assert(sequence == 1 && response[2] == 1);

    assert(!force_feedback_script_status_report_encode(&runtime, FORCE_FEEDBACK_RUNTIME_ACTIVE,
                                                       &sequence, response, sizeof(response) - 1));
    assert(!force_feedback_script_status_report_encode(NULL, FORCE_FEEDBACK_RUNTIME_ACTIVE,
                                                       &sequence, response, sizeof(response)));
    assert(!force_feedback_script_status_report_encode(&runtime, FORCE_FEEDBACK_RUNTIME_ACTIVE,
                                                       NULL, response, sizeof(response)));
    assert(!force_feedback_script_status_report_encode(&runtime, FORCE_FEEDBACK_RUNTIME_ACTIVE,
                                                       &sequence, NULL, sizeof(response)));
}

static void test_encodes_timing_before_writable_variables(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    uint8_t sequence = 7;
    uint8_t response[FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE] = {0};
    static const uint8_t expected_envelope[] = {0x25, 0, 8, 0x31, 7};

    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT; index++) {
        runtime.variables[index] = UINT32_C(0x10203000) + index;
    }
    assert(force_feedback_script_values_report_encode(&runtime, &sequence, response,
                                                      sizeof(response)));
    assert(sequence == 8);
    assert(memcmp(response, expected_envelope, sizeof(expected_envelope)) == 0);
    for (uint8_t index = 0; index < 4; index++) {
        assert(decode_value(response + 5 + index * 4u) == runtime.variables[8 + index]);
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_WRITABLE_VARIABLE_COUNT; index++) {
        assert(decode_value(response + 21 + index * 4u) == runtime.variables[index]);
    }

    assert(!force_feedback_script_values_report_encode(&runtime, &sequence, response,
                                                       sizeof(response) - 1));
    assert(
        !force_feedback_script_values_report_encode(NULL, &sequence, response, sizeof(response)));
    assert(!force_feedback_script_values_report_encode(&runtime, NULL, response, sizeof(response)));
    assert(
        !force_feedback_script_values_report_encode(&runtime, &sequence, NULL, sizeof(response)));
}

int main(void) {
    test_encodes_complete_slot_status_response();
    test_encodes_timing_before_writable_variables();
    return 0;
}
