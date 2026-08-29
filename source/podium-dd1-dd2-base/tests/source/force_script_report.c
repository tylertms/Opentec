#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_report.h"

static uint32_t decode_value(const uint8_t input[4]) {
    return (uint32_t)input[0] | (uint32_t)input[1] << 8u | (uint32_t)input[2] << 16u |
           (uint32_t)input[3] << 24u;
}

static void test_encodes_axis_group_and_padding(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    uint8_t sequence = 10;
    uint8_t response[FORCE_FEEDBACK_SCRIPT_AXES_RESPONSE_SIZE];
    static const uint8_t expected_envelope[] = {0x25, 0, 11, 0x2a, 8};

    memset(response, 0xa5, sizeof(response));
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT; index++) {
        runtime.axes[index] = UINT32_C(0xf0e0d000) + index;
    }
    assert(
        force_feedback_script_axes_report_encode(&runtime, &sequence, response, sizeof(response)));
    assert(sequence == 11);
    assert(memcmp(response, expected_envelope, sizeof(expected_envelope)) == 0);
    assert(response[5] == 0);
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_AXIS_REPORT_COUNT; index++) {
        assert(decode_value(response + 6 + index * 4u) == runtime.axes[index]);
    }
    for (uint8_t index = 38; index < sizeof(response); index++) {
        assert(response[index] == 0);
    }

    assert(!force_feedback_script_axes_report_encode(&runtime, &sequence, response,
                                                     sizeof(response) - 1));
    assert(!force_feedback_script_axes_report_encode(NULL, &sequence, response, sizeof(response)));
    assert(!force_feedback_script_axes_report_encode(&runtime, NULL, response, sizeof(response)));
    assert(!force_feedback_script_axes_report_encode(&runtime, &sequence, NULL, sizeof(response)));
}

static void test_encodes_ten_consecutive_samples(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    uint8_t sequence = 3;
    uint8_t response[FORCE_FEEDBACK_SCRIPT_SAMPLES_RESPONSE_SIZE] = {0};
    static const uint8_t expected_envelope[] = {0x25, 0, 4, 0x2b, 4};

    for (uint16_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_COUNT; index++) {
        runtime.samples.values[501 + index] = UINT32_C(0x80706050) + index;
    }
    assert(force_feedback_script_samples_report_encode(&runtime, 501, &sequence, response,
                                                       sizeof(response)));
    assert(sequence == 4);
    assert(memcmp(response, expected_envelope, sizeof(expected_envelope)) == 0);
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_REPORT_COUNT; index++) {
        assert(decode_value(response + 5 + index * 4u) == runtime.samples.values[501 + index]);
    }

    assert(!force_feedback_script_samples_report_encode(&runtime, 502, &sequence, response,
                                                        sizeof(response)));
    assert(!force_feedback_script_samples_report_encode(&runtime, 0, &sequence, response,
                                                        sizeof(response) - 1));
    assert(!force_feedback_script_samples_report_encode(NULL, 0, &sequence, response,
                                                        sizeof(response)));
    assert(!force_feedback_script_samples_report_encode(&runtime, 0, NULL, response,
                                                        sizeof(response)));
    assert(!force_feedback_script_samples_report_encode(&runtime, 0, &sequence, NULL,
                                                        sizeof(response)));
}

static void test_encodes_slot_details(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    ForceFeedbackScriptSlot *slot = &runtime.slots[14];
    uint8_t sequence = 12;
    uint8_t response[FORCE_FEEDBACK_SCRIPT_SLOT_RESPONSE_SIZE] = {0};
    static const uint8_t expected_envelope[] = {0x25, 0, 13, 0x23, 5};

    slot->state = FORCE_FEEDBACK_SCRIPT_SLOT_FAULT;
    slot->values[0] = UINT32_C(0x10203040);
    slot->values[1] = UINT32_C(0x50607080);
    slot->values[2] = UINT32_C(0x90a0b0c0);
    slot->values[3] = UINT32_C(0xd0e0f001);
    slot->average_rate = UINT32_C(0x11223344);
    slot->delta_rate = UINT32_C(0x55667788);
    slot->execution_count = UINT32_C(0x99aabbcc);
    slot->tick_snapshot = UINT32_C(0xddeeff00);

    assert(force_feedback_script_slot_report_encode(&runtime, 14, &sequence, response,
                                                    sizeof(response)));
    assert(sequence == 13);
    assert(memcmp(response, expected_envelope, sizeof(expected_envelope)) == 0);
    assert(response[5] == 14);
    assert(response[6] == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT);
    for (uint8_t index = 0; index < 4; index++) {
        assert(decode_value(response + 7 + index * 4u) == slot->values[index]);
    }
    assert(decode_value(response + 23) == slot->average_rate);
    assert(decode_value(response + 27) == slot->delta_rate);
    assert(decode_value(response + 31) == slot->execution_count);
    assert(decode_value(response + 35) == slot->tick_snapshot);

    memset(response, 0xa5, sizeof(response));
    assert(force_feedback_script_slot_report_encode(&runtime, 15, &sequence, response,
                                                    sizeof(response)));
    assert(response[0] == 0x25 && response[2] == 14 && response[3] == 0x23 && response[4] == 5);
    for (uint8_t index = 5; index < sizeof(response); index++) {
        assert(response[index] == 0);
    }
    assert(!force_feedback_script_slot_report_encode(&runtime, 16, &sequence, response,
                                                     sizeof(response)));
    assert(!force_feedback_script_slot_report_encode(&runtime, 0, &sequence, response,
                                                     sizeof(response) - 1));
    assert(
        !force_feedback_script_slot_report_encode(NULL, 0, &sequence, response, sizeof(response)));
    assert(
        !force_feedback_script_slot_report_encode(&runtime, 0, NULL, response, sizeof(response)));
    assert(
        !force_feedback_script_slot_report_encode(&runtime, 0, &sequence, NULL, sizeof(response)));
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
    test_encodes_axis_group_and_padding();
    test_encodes_ten_consecutive_samples();
    test_encodes_slot_details();
    test_encodes_complete_slot_status_response();
    test_encodes_timing_before_writable_variables();
    return 0;
}
