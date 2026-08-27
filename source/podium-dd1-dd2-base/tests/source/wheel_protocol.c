#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_mode_one.h"
#include "wheel/protocol.h"

static void mark_ready(uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    packet[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
}

static void synchronize(WheelProtocol *protocol, uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    mark_ready(request);
    wheel_protocol_accept(protocol, request);
    assert(protocol->phase == WHEEL_PROTOCOL_SYNCHRONIZING);
    assert(wheel_protocol_response(protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] == 0);

    wheel_protocol_accept(protocol, request);
    assert(protocol->phase == WHEEL_PROTOCOL_ACKNOWLEDGING);
    assert(wheel_protocol_response(protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] ==
           WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);

    wheel_protocol_accept(protocol, request);
    assert(protocol->phase == WHEEL_PROTOCOL_SELECTING);
}

static void select_mode(WheelProtocol *protocol, uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE],
                        uint8_t mode) {
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[1] = mode;
    wheel_protocol_accept(protocol, request);
}

static void test_synchronizes_and_selects_mode(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);

    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    assert(protocol.phase == WHEEL_PROTOCOL_SELECTED);
    assert(protocol.mode == 1);
    assert(wheel_protocol_response(&protocol)[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0x9a);
    assert(wheel_protocol_message_valid(wheel_protocol_response(&protocol)));
}

static void test_selects_scan_variants(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);

    request[0] = WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(protocol.mode == WHEEL_MODE_SCAN_PRIMARY);

    wheel_protocol_init(&protocol);
    memset(request, 0, sizeof(request));
    synchronize(&protocol, request);
    request[0] = WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY);
    assert(protocol.mode == WHEEL_MODE_SCAN_SECONDARY);
}

static void test_detects_authentication_command(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 0x10);

    request[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(protocol.mode == 0x10);
}

static void test_restarts_synchronization_when_ready_drops(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    mark_ready(request);
    wheel_protocol_accept(&protocol, request);
    wheel_protocol_accept(&protocol, (uint8_t[WHEEL_PROTOCOL_PACKET_SIZE]){0});

    assert(protocol.phase == WHEEL_PROTOCOL_WAITING);
    assert(protocol.mode == WHEEL_MODE_UNKNOWN);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] == 0);
}

static void test_captures_raw_active_requests(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
        request[index] = index;
    }
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_protocol_request(&protocol) != 0);
    assert(memcmp(wheel_protocol_request(&protocol), request, WHEEL_PROTOCOL_SNAPSHOT_SIZE) == 0);
    assert(wheel_protocol_request_changed(&protocol));
    assert(!wheel_protocol_request_changed(&protocol));

    wheel_protocol_accept(&protocol, request);
    assert(!wheel_protocol_request_changed(&protocol));
    request[29]++;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);
    assert(wheel_protocol_request_changed(&protocol));
}

static void test_builds_mode_one_active_response(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    const WheelPacketModeOneOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .display_state = {0x44, 0x55},
        .link_status = {0x66, 0x77},
    };
    wheel_protocol_init(&protocol);
    wheel_protocol_set_mode_one_output(&protocol, &output);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    const uint8_t *response = wheel_protocol_response(&protocol);
    const uint8_t expected[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };
    assert(memcmp(response, expected, sizeof(expected)) == 0);
    assert(wheel_protocol_message_valid(response));
}

static void test_rejects_out_of_range_mode(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, WHEEL_MODE_MAXIMUM + 1);

    assert(protocol.phase == WHEEL_PROTOCOL_UNSUPPORTED);
    assert(protocol.mode == WHEEL_MODE_UNKNOWN);
}

static void test_crc8_vectors(void) {
    uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    packet[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    assert(wheel_protocol_message_checksum(packet) == 0x9a);

    for (uint8_t index = 0; index < WHEEL_PROTOCOL_CONTENT_SIZE; index++) {
        packet[index] = index;
    }
    assert(wheel_protocol_message_checksum(packet) == 0x21);
}

int main(void) {
    test_synchronizes_and_selects_mode();
    test_selects_scan_variants();
    test_detects_authentication_command();
    test_restarts_synchronization_when_ready_drops();
    test_captures_raw_active_requests();
    test_builds_mode_one_active_response();
    test_rejects_out_of_range_mode();
    test_crc8_vectors();
    return 0;
}
