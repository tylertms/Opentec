#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "pedal/protocol.h"

static void test_selects_protocol(void) {
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0x15) == PEDAL_PROTOCOL_V3);
    assert(pedal_protocol_select(PEDAL_DEVICE_V4, 0x26) == PEDAL_PROTOCOL_V4);
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0x14) == PEDAL_PROTOCOL_LEGACY);
    assert(pedal_protocol_select(PEDAL_DEVICE_V4, 0x25) == PEDAL_PROTOCOL_LEGACY);
    assert(pedal_protocol_select(PEDAL_DEVICE_NONE, 0x15) == PEDAL_PROTOCOL_REDISCOVER);
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0) == PEDAL_PROTOCOL_REDISCOVER);
    assert(pedal_protocol_select(PEDAL_DEVICE_INVALID, 0x15) == PEDAL_PROTOCOL_REDISCOVER);
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0xff) == PEDAL_PROTOCOL_REDISCOVER);
}

static void test_builds_legacy_requests(void) {
    assert(pedal_legacy_request(PEDAL_LEGACY_AXIS_1, 0x7f, 0x3a) == 0x40);
    assert(pedal_legacy_request(PEDAL_LEGACY_AXIS_2, 0x7f, 0x3a) == 0xbf);
    assert(pedal_legacy_request(PEDAL_LEGACY_AXIS_3, 0x7f, 0x3a) == 0xfa);
    assert(pedal_legacy_request(PEDAL_LEGACY_AUXILIARY, 0x7f, 0x3a) == 0);
}

static void test_applies_legacy_responses(void) {
    PedalInput input = {
        .axes = {1, 2, 3},
        .auxiliary = 4,
    };

    pedal_legacy_apply_response(PEDAL_LEGACY_AXIS_1, 0xa5, false, &input);
    pedal_legacy_apply_response(PEDAL_LEGACY_AXIS_2, 0x00, false, &input);
    pedal_legacy_apply_response(PEDAL_LEGACY_AXIS_3, 0xff, false, &input);
    assert(input.axes[0] == 0x5a00);
    assert(input.axes[1] == 0xff00);
    assert(input.axes[2] == 0);

    pedal_legacy_apply_response(PEDAL_LEGACY_AUXILIARY, 0x35, true, &input);
    assert(input.auxiliary == 4);
    pedal_legacy_apply_response(PEDAL_LEGACY_AUXILIARY, 0x35, false, &input);
    assert(input.auxiliary == 0x35);
}

static void test_builds_v3_handshakes(void) {
    PedalFrame frame;
    pedal_v3_build_handshake(false, &frame);
    assert(frame.type == 2);
    assert(frame.payload[0] == 0xff);
    assert(frame.payload[1] == 0);
    for (uint8_t index = 2; index < PEDAL_FRAME_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }

    pedal_v3_build_handshake(true, &frame);
    assert(frame.type == 2);
    assert(frame.payload[0] == 0);
    assert(frame.payload[1] == 0xff);
}

static void test_builds_v3_status(void) {
    const PedalProtocolStatus status = {
        .value = 0x11,
        .first = 0x22,
        .second = 0x33,
        .scale = 0x44,
    };
    PedalFrame frame;
    pedal_v3_build_status(&status, &frame);

    assert(frame.type == 0);
    assert(frame.payload[0] == 0x11);
    assert(frame.payload[1] == 0x22);
    assert(frame.payload[2] == 0x33);
    assert(frame.payload[3] == 0x44);
    for (uint8_t index = 4; index < PEDAL_FRAME_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }
}

static void test_builds_v3_control_sequence(void) {
    uint8_t pending = PEDAL_V3_CONTROL_UP | PEDAL_V3_CONTROL_ENABLE | PEDAL_V3_CONTROL_AUTOMATIC;
    PedalFrame frame;

    pending = pedal_v3_build_control(pending, &frame);
    assert(frame.type == 2);
    assert(frame.payload[0] == 0xff);
    assert(frame.payload[1] == 0);
    assert(frame.payload[2] == 0xff);
    assert(frame.payload[3] == 0);
    assert(frame.payload[4] == 0xff);
    assert(frame.payload[5] == 0);
    assert(pending == PEDAL_V3_CONTROL_AUTOMATIC);

    pending = pedal_v3_build_control(pending, &frame);
    assert(frame.payload[0] == 0xff);
    assert(frame.payload[1] == 0);
    assert(frame.payload[2] == 0);
    assert(frame.payload[3] == 0);
    assert(frame.payload[4] == 0);
    assert(frame.payload[5] == 0xff);
    assert(pending == 0);
}

static void test_builds_v3_input_configuration_and_keepalive(void) {
    const uint8_t values[PEDAL_INPUT_AXIS_COUNT] = {1, 2, 3};
    PedalFrame frame;

    pedal_v3_build_input_command(values, &frame);
    assert(frame.type == 3);
    assert(frame.payload[0] == 1);
    assert(frame.payload[1] == 2);
    assert(frame.payload[2] == 3);
    for (uint8_t index = 3; index < PEDAL_FRAME_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }

    pedal_v3_build_configuration(79, false, false, &frame);
    assert(frame.type == 6);
    assert(frame.payload[0] == 8);
    assert(frame.payload[1] == 0);
    pedal_v3_build_configuration(79, true, true, &frame);
    assert(frame.payload[0] == 16);
    assert(frame.payload[1] == 0xff);

    pedal_v3_build_keepalive(&frame);
    assert(frame.type == 0x10);
    for (uint8_t index = 0; index < PEDAL_FRAME_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }
}

int main(void) {
    test_selects_protocol();
    test_builds_legacy_requests();
    test_applies_legacy_responses();
    test_builds_v3_handshakes();
    test_builds_v3_status();
    test_builds_v3_control_sequence();
    test_builds_v3_input_configuration_and_keepalive();
    return 0;
}
