#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/protocol.h"

static void mark_ready(uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    packet[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
}

static void test_acknowledges_and_selects_plain_mode(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);

    mark_ready(request);
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SELECTING);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] ==
           WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);

    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[1] = 1;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(protocol.mode == 1);
    assert(wheel_protocol_response(&protocol)[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0x9a);
    assert(wheel_protocol_message_valid(wheel_protocol_response(&protocol)));
}

static void test_selects_legacy_scan_variants(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    mark_ready(request);
    wheel_protocol_accept(&protocol, request);

    request[0] = WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(protocol.mode == WHEEL_MODE_SCAN_PRIMARY);

    wheel_protocol_init(&protocol);
    wheel_protocol_accept(&protocol, request);
    request[0] = WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY);
    assert(protocol.mode == WHEEL_MODE_SCAN_SECONDARY);
}

static void test_holds_authenticated_modes_for_handshake(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    mark_ready(request);
    wheel_protocol_accept(&protocol, request);
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[1] = 0x10;
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(protocol.mode == 0x10);
    assert(wheel_protocol_mode_requires_authentication(0x10));
    assert(!wheel_protocol_mode_requires_authentication(0x09));
}

static void test_resets_when_wheel_drops_ready(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    mark_ready(request);
    wheel_protocol_accept(&protocol, request);
    wheel_protocol_accept(&protocol, (uint8_t[WHEEL_PROTOCOL_PACKET_SIZE]){0});

    assert(protocol.phase == WHEEL_PROTOCOL_WAITING);
    assert(protocol.mode == WHEEL_MODE_BOOT);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] == 0);
}

static void test_decodes_packet_input_and_builds_display_output(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    mark_ready(request);
    wheel_protocol_accept(&protocol, request);
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[1] = 1;
    wheel_protocol_accept(&protocol, request);

    const WheelProtocolOutput output = {
        .display_segments = {0x12, 0x34, 0x56},
        .display_value = 0x78,
        .display_status = 0x9a,
        .legacy_axes = {0xbc, 0xde},
    };
    wheel_protocol_set_output(&protocol, &output);
    const uint8_t *response = wheel_protocol_response(&protocol);
    assert(response[2] == 0x12);
    assert(response[3] == 0x34);
    assert(response[4] == 0x56);
    assert(response[5] == 0x78);
    assert(response[6] == 0x9a);
    assert(response[7] == 0xbc);
    assert(response[8] == 0xde);
    assert(wheel_protocol_message_valid(response));

    for (uint8_t sample = 0; sample < 3; sample++) {
        request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
        request[2] = 0xa5;
        request[3] = 0x5a;
        request[4] = 0x3c;
        request[5] = 0x11;
        request[6] = 0x22;
        request[7] = 0xfe;
        for (uint8_t index = 0; index < 8; index++) {
            request[8 + index] = (uint8_t)(0x30 + index);
        }
        request[18] = 0x44;
        request[19] = 0x55;
        request[20] = 0x66;
        request[21] = 1;
        request[28] = 0x77;
        request[30] = 0x88;
        request[31] = 0x99;
        request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
        wheel_protocol_accept(&protocol, request);
    }

    const WheelProtocolInput *input = wheel_protocol_input(&protocol);
    assert(input != 0);
    assert(input->buttons[0] == 0xa5);
    assert(input->buttons[1] == 0x5a);
    assert(input->buttons[2] == 0x3c);
    assert(input->axis_outputs[0] == 0x11);
    assert(input->axis_outputs[1] == 0x22);
    assert(input->motion == -2);
    for (uint8_t index = 0; index < 8; index++) {
        assert(input->controls[index] == (uint8_t)(0x30 + index));
    }
    assert(input->axis_values[0] == 0x44);
    assert(input->axis_values[1] == 0x55);
    assert(input->mode_buttons == 0x66);
    assert(input->axis_report_enabled);
    assert(input->capability_flags == 0x8877);
    assert(input->axis_limit == 0x99);
}

int main(void) {
    test_acknowledges_and_selects_plain_mode();
    test_selects_legacy_scan_variants();
    test_holds_authenticated_modes_for_handshake();
    test_resets_when_wheel_drops_ready();
    test_decodes_packet_input_and_builds_display_output();
    return 0;
}
