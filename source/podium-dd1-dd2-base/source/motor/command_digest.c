#include "motor/command_digest.h"

#include <stdint.h>

/**
 * @brief Derives the motor-command status digest.
 *
 * XOR-folds the status source into six identity bytes and clears the final two digest bytes.
 *
 * @param[in] source Sixteen-byte motor-command status source.
 * @param[out] digest Eight-byte identity digest.
 */
void motor_command_digest_encode(const uint8_t source[MOTOR_COMMAND_DIGEST_SOURCE_SIZE],
                                 uint8_t digest[MOTOR_COMMAND_DIGEST_SIZE]) {
    for (uint8_t index = 0; index < 4; index++) {
        digest[index] = source[index + 4] ^ source[index + 8] ^ source[index + 12];
    }
    digest[4] = source[2] ^ source[12];
    digest[5] = source[3] ^ source[13];
    digest[6] = 0;
    digest[7] = 0;
}
