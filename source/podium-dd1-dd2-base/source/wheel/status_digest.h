#ifndef OPENTEC_BASE_WHEEL_STATUS_DIGEST_H
#define OPENTEC_BASE_WHEEL_STATUS_DIGEST_H

#include <stdint.h>

enum {
    WHEEL_STATUS_DIGEST_SOURCE_SIZE = 16,
    WHEEL_STATUS_DIGEST_SIZE = 8,
};

void wheel_status_digest_encode(const uint8_t source[WHEEL_STATUS_DIGEST_SOURCE_SIZE],
                                uint8_t digest[WHEEL_STATUS_DIGEST_SIZE]);

#endif
