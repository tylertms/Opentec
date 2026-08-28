#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/status_digest.h"

static void test_encodes_status_digest(void) {
    static const uint8_t source[WHEEL_STATUS_DIGEST_SOURCE_SIZE] = {
        0x22, 0x44, 0x10, 0x80, 0x12, 0x34, 0x56, 0x78,
        0x9a, 0xbc, 0xde, 0xf0, 0x11, 0x22, 0x33, 0x44,
    };
    static const uint8_t expected[WHEEL_STATUS_DIGEST_SIZE] = {
        0x99, 0xaa, 0xbb, 0xcc, 0x01, 0xa2, 0x00, 0x00,
    };
    uint8_t digest[WHEEL_STATUS_DIGEST_SIZE];

    wheel_status_digest_encode(source, digest);

    assert(memcmp(digest, expected, sizeof(expected)) == 0);
}

int main(void) {
    test_encodes_status_digest();
    return 0;
}
