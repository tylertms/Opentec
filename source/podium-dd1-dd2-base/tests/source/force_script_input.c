#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_input.h"

static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static void prepare_unused_sample_packet(uint8_t *packet) {
    memset(packet, 0, FORCE_FEEDBACK_SCRIPT_PACKET_SIZE);
    packet[0] = 0x0b;
    for (uint8_t record = 0; record < FORCE_FEEDBACK_SCRIPT_SAMPLE_UPDATE_COUNT; record++) {
        write_u16(&packet[4 + record * 6], UINT16_MAX);
    }
}

static void test_initializes_sample_table(void) {
    ForceFeedbackScriptSamples samples;
    memset(&samples, 0, sizeof(samples));

    force_feedback_script_samples_init(&samples);
    for (uint16_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT; index++) {
        assert(samples.values[index] == UINT32_MAX);
    }
    force_feedback_script_samples_init(NULL);
}

static void test_applies_selected_sample_records(void) {
    ForceFeedbackScriptSamples samples;
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_samples_init(&samples);
    prepare_unused_sample_packet(packet);
    write_u16(&packet[4], 0);
    write_u32(&packet[6], 0x12345678);
    write_u16(&packet[10], 511);
    write_u32(&packet[12], 0x89abcdef);

    assert(force_feedback_script_samples_apply(&samples, packet, sizeof(packet)));
    assert(samples.values[0] == 0x12345678);
    assert(samples.values[1] == UINT32_MAX);
    assert(samples.values[511] == 0x89abcdef);
}

static void test_rejects_all_samples_before_writing(void) {
    ForceFeedbackScriptSamples samples;
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_samples_init(&samples);
    prepare_unused_sample_packet(packet);
    write_u16(&packet[4], 3);
    write_u32(&packet[6], 0x11223344);
    write_u16(&packet[58], 512);

    ForceFeedbackScriptSamples before = samples;
    assert(!force_feedback_script_samples_apply(&samples, packet, sizeof(packet)));
    assert(memcmp(&samples, &before, sizeof(samples)) == 0);

    write_u16(&packet[58], UINT16_MAX);
    assert(!force_feedback_script_samples_apply(&samples, packet, sizeof(packet) - 1));
    packet[0] = 0x0c;
    assert(!force_feedback_script_samples_apply(&samples, packet, sizeof(packet)));
    assert(!force_feedback_script_samples_apply(NULL, packet, sizeof(packet)));
    assert(!force_feedback_script_samples_apply(&samples, NULL, sizeof(packet)));
}

static void test_applies_active_live_inputs(void) {
    ForceFeedbackScriptInputs inputs;
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {
        [0] = 0x0e,
        [4] = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE,
        [9] = FORCE_FEEDBACK_SCRIPT_INPUT_POSITION,
        [18] = FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED,
        [27] = 7,
    };
    force_feedback_script_inputs_init(&inputs);
    inputs.slots[1] = (ForceFeedbackScriptInputSlot){.status = 9, .value = 10, .duration = 11};
    write_u16(&packet[5], 0x2345);
    write_u16(&packet[7], 0x6789);
    write_u32(&packet[10], 0x12345678);
    write_u32(&packet[14], 0x11223344);
    write_u32(&packet[28], 0xaabbccdd);
    write_u32(&packet[32], 0x55667788);

    assert(force_feedback_script_inputs_apply(&inputs, 0xfffffff0, packet, sizeof(packet)));
    assert(inputs.status == FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE);
    assert(inputs.deadline == 0x2335);
    assert(inputs.sample_count == 0x6789);
    assert(inputs.slots[0].status == FORCE_FEEDBACK_SCRIPT_INPUT_POSITION);
    assert(inputs.slots[0].value == 0x12345678);
    assert(inputs.slots[0].duration == 0x11223344);
    assert(inputs.position_value == 0x12345678);
    assert(inputs.slots[1].status == 9);
    assert(inputs.slots[1].value == 10);
    assert(inputs.slots[1].duration == 11);
    assert(inputs.slots[2].status == 7);
    assert(inputs.slots[2].value == 0xaabbccdd);
    assert(inputs.slots[2].duration == 0x55667788);
}

static void test_only_updates_status_for_inactive_packet(void) {
    ForceFeedbackScriptInputs inputs = {
        .status = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE,
        .deadline = 1,
        .sample_count = 2,
        .slots = {{3, 4, 5}, {6, 7, 8}, {9, 10, 11}},
        .position_value = 12,
    };
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {[0] = 0x0e, [4] = 2};
    ForceFeedbackScriptInputs expected = inputs;
    expected.status = 2;

    assert(force_feedback_script_inputs_apply(&inputs, 100, packet, sizeof(packet)));
    assert(memcmp(&inputs, &expected, sizeof(inputs)) == 0);
}

static void test_rejects_invalid_live_input_packets(void) {
    ForceFeedbackScriptInputs inputs;
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE] = {[0] = 0x0e};
    force_feedback_script_inputs_init(&inputs);

    assert(!force_feedback_script_inputs_apply(&inputs, 0, packet, sizeof(packet) - 1));
    packet[0] = 0x0d;
    assert(!force_feedback_script_inputs_apply(&inputs, 0, packet, sizeof(packet)));
    assert(!force_feedback_script_inputs_apply(NULL, 0, packet, sizeof(packet)));
    assert(!force_feedback_script_inputs_apply(&inputs, 0, NULL, sizeof(packet)));
    force_feedback_script_inputs_init(NULL);
}

int main(void) {
    test_initializes_sample_table();
    test_applies_selected_sample_records();
    test_rejects_all_samples_before_writing();
    test_applies_active_live_inputs();
    test_only_updates_status_for_inactive_packet();
    test_rejects_invalid_live_input_packets();
    return 0;
}
