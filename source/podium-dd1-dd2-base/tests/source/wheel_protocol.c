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

int main(void) {
    test_acknowledges_and_selects_plain_mode();
    test_selects_legacy_scan_variants();
    test_holds_authenticated_modes_for_handshake();
    test_resets_when_wheel_drops_ready();
    return 0;
}
