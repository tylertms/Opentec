#include "motor/link.h"

#include <stddef.h>
#include <stdint.h>

#include "fsl_crc.h"

/**
 * @brief Calculates the official motor-link CRC with the NXP CRC peripheral.
 *
 * The hardware engine is configured for the recovered sixteen-bit reflected protocol polynomial.
 *
 * @param data Motor-link frame body bytes.
 * @param length Body byte count.
 * @return CRC-16 result using polynomial 0x1021 and seed zero.
 */
uint16_t motor_link_crc_calculate(const uint8_t *data, size_t length) {
    crc_config_t config;
    CRC_GetDefaultConfig(&config);
    config.seed = 0U;
    CRC_Init(CRC0, &config);
    CRC_WriteData(CRC0, data, length);
    return CRC_Get16bitResult(CRC0);
}

/**
 * @brief Calculates the CRC and decodes one official motor-link frame.
 *
 * Hardware CRC output is supplied to the pure boundary and payload decoder.
 *
 * @param input Complete received motor-link frame.
 * @param frame Decoded frame type and payload.
 * @return Boundary, checksum, or valid result.
 */
MotorLinkFrameResult motor_link_frame_decode(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                             MotorLinkFrame *frame) {
    uint16_t checksum = motor_link_crc_calculate(input + 1U, MOTOR_LINK_CHECKSUM_INPUT_SIZE);
    return motor_link_frame_decode_checked(input, checksum, frame);
}

/**
 * @brief Encodes one official motor position response with a hardware CRC.
 *
 * The response body is packed first, then the hardware checksum completes the wire frame.
 *
 * @param report Current motor response values and replay flag.
 * @param output Complete thirteen-byte motor-link frame.
 */
void motor_link_position_frame_encode(const MotorLinkPositionReport *report,
                                      uint8_t output[MOTOR_LINK_FRAME_SIZE]) {
    motor_link_position_frame_prepare(report, output);
    uint16_t checksum = motor_link_crc_calculate(output + 1U, MOTOR_LINK_CHECKSUM_INPUT_SIZE);
    motor_link_frame_checksum_write(output, checksum);
}
