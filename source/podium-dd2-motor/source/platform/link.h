#ifndef OPENTEC_MOTOR_LINK_H
#define OPENTEC_MOTOR_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "link/frame.h"

/**
 * @brief Calculates the CRC for a motor-link frame body.
 *
 * The hardware CRC peripheral processes the supplied bytes with the protocol polynomial and seed
 * used by the motor link.
 *
 * @param[in] data Motor-link frame body bytes.
 * @param[in] length Number of body bytes to process.
 * @return Protocol CRC-16 value.
 */
uint16_t motor_link_crc_calculate(const uint8_t *data, size_t length);

/**
 * @brief Validates and decodes one received motor-link frame.
 *
 * Boundary bytes are checked before the hardware CRC is calculated, and the validated payload is
 * decoded into the supplied frame object.
 *
 * @param[in] input Complete received motor-link frame.
 * @param[out] frame Decoded frame type and payload.
 * @return Boundary, checksum, or valid frame result.
 */
MotorLinkFrameResult motor_link_frame_decode(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                             MotorLinkFrame *frame);

/**
 * @brief Encodes one motor position response frame.
 *
 * The response body is packed into the output buffer and completed with the protocol CRC.
 *
 * @param[in] report Current motor response values and replay flag.
 * @param[out] output Complete motor-link frame.
 */
void motor_link_position_frame_encode(const MotorLinkPositionReport *report,
                                      uint8_t output[MOTOR_LINK_FRAME_SIZE]);

#endif
