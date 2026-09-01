#include "platform/link.h"

#include <stddef.h>
#include <stdint.h>

#include "fsl_crc.h"

uint16_t motor_link_crc_calculate(const uint8_t *data, size_t length) {
    crc_config_t config;
    CRC_GetDefaultConfig(&config);
    config.seed = 0U;
    CRC_Init(CRC0, &config);
    CRC_WriteData(CRC0, data, length);
    return CRC_Get16bitResult(CRC0);
}

MotorLinkFrameResult motor_link_frame_decode(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                             MotorLinkFrame *frame) {
    if (!motor_link_frame_boundaries_valid(input)) {
        return MOTOR_LINK_FRAME_INVALID_BOUNDARY;
    }
    uint16_t checksum = motor_link_crc_calculate(input + 1U, MOTOR_LINK_CHECKSUM_INPUT_SIZE);
    return motor_link_frame_decode_checked(input, checksum, frame);
}

void motor_link_position_frame_encode(const MotorLinkPositionReport *report,
                                      uint8_t output[MOTOR_LINK_FRAME_SIZE]) {
    motor_link_position_frame_prepare(report, output);
    uint16_t checksum = motor_link_crc_calculate(output + 1U, MOTOR_LINK_CHECKSUM_INPUT_SIZE);
    motor_link_frame_checksum_write(output, checksum);
}
