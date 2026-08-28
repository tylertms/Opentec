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

int main(void) {
    test_encodes_digest_request();
    test_encodes_control_responses();
    return 0;
}
