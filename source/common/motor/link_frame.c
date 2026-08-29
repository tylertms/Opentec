#include "common/motor/link_frame.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    FRAME_START = 0x7b,
    FRAME_END = 0x7d,
    FRAME_CHECKSUM_OFFSET = 10,
};

static uint16_t read_uint16(const uint8_t *input) {
    return (uint16_t)input[0] | (uint16_t)input[1] << 8U;
}

/**
 * @brief Validates and decodes one official thirteen-byte motor-link frame.
 * @param input Complete received frame.
 * @param checksum CRC peripheral result for frame bytes one through nine.
 * @param frame Decoded frame type and payload.
 * @return Boundary, checksum, or valid result.
 */
MotorLinkFrameResult motor_link_frame_decode_checked(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                                     uint16_t checksum, MotorLinkFrame *frame) {
    if (input[0] != FRAME_START || input[MOTOR_LINK_FRAME_SIZE - 1U] != FRAME_END) {
        return MOTOR_LINK_FRAME_INVALID_BOUNDARY;
    }
    if (read_uint16(input + FRAME_CHECKSUM_OFFSET) != checksum) {
        return MOTOR_LINK_FRAME_INVALID_CHECKSUM;
    }

    frame->type = input[1];
    for (uint8_t index = 0U; index < MOTOR_LINK_PAYLOAD_SIZE; ++index) {
        frame->payload[index] = input[index + 2U];
    }
    return MOTOR_LINK_FRAME_VALID;
}

/**
 * @brief Decodes the official live-force motor-link payload.
 * @param frame Decoded motor-link frame.
 * @param command Live center and force output fields.
 * @return True when the frame is a live-force command.
 */
bool motor_link_force_command_decode(const MotorLinkFrame *frame, MotorLinkForceCommand *command) {
    if (frame->type != MOTOR_LINK_FORCE_TYPE) {
        return false;
    }
    command->center = (int16_t)read_uint16(frame->payload);
    command->positive = frame->payload[2] != 0U;
    command->primary = read_uint16(frame->payload + 3U);
    command->secondary = (int16_t)read_uint16(frame->payload + 5U);
    return true;
}

/**
 * @brief Decodes the official status and effect-command motor-link payload.
 * @param frame Decoded motor-link frame.
 * @param command Status byte and seven-byte force-feedback command.
 * @return True when the frame is a status command.
 */
bool motor_link_status_command_decode(const MotorLinkFrame *frame,
                                      MotorLinkStatusCommand *command) {
    if (frame->type != MOTOR_LINK_STATUS_TYPE) {
        return false;
    }
    command->status = frame->payload[0];
    for (uint8_t index = 0U; index < sizeof(command->command); ++index) {
        command->command[index] = frame->payload[index + 1U];
    }
    return true;
}
