#ifndef OPENTEC_BASE_MOTOR_COMMAND_DIGEST_H
#define OPENTEC_BASE_MOTOR_COMMAND_DIGEST_H

#include <stdint.h>

/** @brief Fixed sizes of the motor-command calibration digest transformation. */
enum {
    MOTOR_COMMAND_DIGEST_SOURCE_SIZE = 16, /**< Number of source bytes consumed by the digest encoder. */
    MOTOR_COMMAND_DIGEST_SIZE = 8, /**< Number of bytes written by the digest encoder. */
};

/**
 * @brief Encodes a motor-command calibration digest.
 *
 * XOR-folds the sixteen-byte source into six digest bytes and clears the final two bytes. Both
 * arrays must provide the sizes named by the public constants.
 *
 * @param[in] source Sixteen-byte calibration source data.
 * @param[out] digest Eight-byte destination digest.
 */
void motor_command_digest_encode(const uint8_t source[MOTOR_COMMAND_DIGEST_SOURCE_SIZE],
                                 uint8_t digest[MOTOR_COMMAND_DIGEST_SIZE]);

#endif
