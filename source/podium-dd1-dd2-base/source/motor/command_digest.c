#include "motor/command_digest.h"

#include <stdint.h>

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
