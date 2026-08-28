#include "wheel/status_digest.h"

#include <stdint.h>

/**
 * @brief Derives the attached-wheel status digest.
 *
 * XOR-folds the status source into six identity bytes and clears the final two digest bytes.
 *
 * @param[in] source Sixteen-byte attached-wheel status source.
 * @param[out] digest Eight-byte identity digest.
 */
void wheel_status_digest_encode(const uint8_t source[WHEEL_STATUS_DIGEST_SOURCE_SIZE],
                                uint8_t digest[WHEEL_STATUS_DIGEST_SIZE]) {
    for (uint8_t index = 0; index < 4; index++) {
        digest[index] = source[index + 4] ^ source[index + 8] ^ source[index + 12];
    }
    digest[4] = source[2] ^ source[12];
    digest[5] = source[3] ^ source[13];
    digest[6] = 0;
    digest[7] = 0;
}
