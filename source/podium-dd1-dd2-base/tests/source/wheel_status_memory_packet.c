#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/status_memory_packet.h"

static void test_encodes_digest_request(void) {
    static const uint8_t expected[] = {
        0x03, 0x00, 0x06, 0x00, 0x07, 0x00, 0x00, 0x00, 0x14, 0xe8, 0xb3,
    };
    uint8_t packet[WHEEL_STATUS_MEMORY_DIGEST_REQUEST_SIZE];

    wheel_status_memory_digest_request_encode(0, 3, false, packet);

    assert(memcmp(packet, expected, sizeof(expected)) == 0);
}

static void test_encodes_control_responses(void) {
    static const uint8_t expected_acknowledgement[] = {0x83, 0, 0, 0xe3, 0x88};
    static const uint8_t expected_reset[] = {0xc0, 0, 0, 0x0a, 0x9a};
    uint8_t packet[WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE];

    wheel_status_memory_acknowledgement_encode(3, packet);
    assert(memcmp(packet, expected_acknowledgement, sizeof(expected_acknowledgement)) == 0);
    wheel_status_memory_sequence_reset_encode(packet);
    assert(memcmp(packet, expected_reset, sizeof(expected_reset)) == 0);
}

static void test_encodes_information_requests(void) {
    static const uint8_t selector_three[] = {
        0x03, 0x00, 0x06, 0x00, 0x05, 0x00, 0x03, 0x00, 0x02, 0x64, 0xe8,
    };
    static const uint8_t selector_four[] = {
        0x03, 0x00, 0x06, 0x00, 0x05, 0x00, 0x04, 0x00, 0x02, 0xe8, 0xed,
    };
    uint8_t packet[WHEEL_STATUS_MEMORY_INFO_REQUEST_SIZE];

    assert(wheel_status_memory_info_request_encode(3, 0, 3, false, packet));
    assert(memcmp(packet, selector_three, sizeof(packet)) == 0);
    assert(wheel_status_memory_info_request_encode(4, 0, 3, false, packet));
    assert(memcmp(packet, selector_four, sizeof(packet)) == 0);
    assert(!wheel_status_memory_info_request_encode(0, 0, 3, false, packet));
    assert(!wheel_status_memory_info_request_encode(10, 0, 3, false, packet));
}

static void test_decodes_digest_response(void) {
    static const uint8_t packet[] = {
        0x03, 0x00, 0x1a, 0x00, 0x87, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x93, 0xb3,
    };
    uint8_t source[WHEEL_STATUS_DIGEST_SOURCE_SIZE];

    assert(wheel_status_memory_digest_response_decode(3, packet, sizeof(packet), source));
    for (uint8_t index = 0; index < sizeof(source); index++) {
        assert(source[index] == index);
    }
}

static void test_rejects_invalid_digest_responses(void) {
    static const uint8_t valid[] = {
        0x03, 0x00, 0x1a, 0x00, 0x87, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x93, 0xb3,
    };
    uint8_t packet[sizeof(valid)];
    uint8_t source[WHEEL_STATUS_DIGEST_SOURCE_SIZE];

    memcpy(packet, valid, sizeof(packet));
    packet[sizeof(packet) - 1] ^= 1;
    assert(!wheel_status_memory_digest_response_decode(3, packet, sizeof(packet), source));
    assert(!wheel_status_memory_digest_response_decode(2, valid, sizeof(valid), source));
    assert(!wheel_status_memory_digest_response_decode(3, valid, 3, source));

    memcpy(packet, valid, sizeof(packet));
    packet[3] = 1;
    assert(!wheel_status_memory_digest_response_decode(3, packet, sizeof(packet), source));
}

int main(void) {
    test_encodes_digest_request();
    test_encodes_control_responses();
    test_encodes_information_requests();
    test_decodes_digest_response();
    test_rejects_invalid_digest_responses();
    return 0;
}
