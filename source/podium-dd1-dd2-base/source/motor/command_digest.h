#ifndef OPENTEC_BASE_MOTOR_COMMAND_DIGEST_H
#define OPENTEC_BASE_MOTOR_COMMAND_DIGEST_H

#include <stdint.h>

enum {
    MOTOR_COMMAND_DIGEST_SOURCE_SIZE = 16,
    MOTOR_COMMAND_DIGEST_SIZE = 8,
};

void motor_command_digest_encode(const uint8_t source[MOTOR_COMMAND_DIGEST_SOURCE_SIZE],
                                 uint8_t digest[MOTOR_COMMAND_DIGEST_SIZE]);

#endif
