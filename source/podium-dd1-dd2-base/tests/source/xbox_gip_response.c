#include "usb/xbox_gip_response.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_advances_and_wraps_response_sequence(void) {
    uint8_t sequence = 1;
    assert(usb_xbox_gip_sequence_take(&sequence) == 1);
    assert(sequence == 2);

    sequence = 254;
    assert(usb_xbox_gip_sequence_take(&sequence) == 254);
    assert(sequence == 255);
    assert(usb_xbox_gip_sequence_take(&sequence) == 1);
    assert(sequence == 1);
}

static void test_encodes_digest_response(void) {
    static const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE] = {
        0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87,
    };
    static const uint8_t expected[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE] = {
        0x02, 0x20, 0x2a, 0x1c, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76,
        0x87, 0xb7, 0x0e, 0x50, 0x0f, 0x03, 0x00, 0x09, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    };
    uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE];

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD1, 6, 0x2a, digest, output);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_maps_base_and_extended_status_modes(void) {
    uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE] = {0};
    uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE];

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD2, 10, 1, digest, output);
    assert(output[14] == 0x64 && output[15] == 0x0f);
    assert(output[24] == 4);

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD1, 29, 1, digest, output);
    assert(output[14] == 0x53 && output[15] == 0x0f);
    assert(output[24] == 5);

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD1, 0, 1, digest, output);
    assert(output[14] == 0 && output[15] == 0);
    assert(output[24] == 4);
}

int main(void) {
    test_advances_and_wraps_response_sequence();
    test_encodes_digest_response();
    test_maps_base_and_extended_status_modes();
    return 0;
}
