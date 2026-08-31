#include "motor/live_frame.h"

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"

enum {
    MOTOR_LIVE_CHECKSUM_OFFSET = 10,
    MOTOR_LIVE_CHECKSUM_INPUT_OFFSET = 1,
    MOTOR_LIVE_CHECKSUM_INPUT_SIZE = 9,
    MOTOR_POSITION_AUXILIARY_DIRECTION = 0x8000,
};

/**
 * @brief Advances a CRC-16/CCITT value by one byte.
 *
 * Applies the non-reflected polynomial 0x1021 from the most-significant bit through the least-
 * significant bit.
 *
 * @param[in] crc CRC value before the byte is consumed.
 * @param[in] byte Next input byte.
 * @return Updated CRC value.
 */
static uint16_t crc16_shift(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) != 0 ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

/**
 * @brief Calculates the motor live-frame checksum.
 *
 * Applies CRC-16/CCITT with a zero seed to the nine payload bytes following the frame identifier.
 *
 * @param[in] input Nine-byte checksum input.
 * @return Calculated CRC-16 value.
 */
static uint16_t checksum(const uint8_t *input) {
    uint16_t crc = 0;
    for (uint8_t index = 0; index < MOTOR_LIVE_CHECKSUM_INPUT_SIZE; index++) {
        crc = crc16_shift(crc, input[index]);
    }
    return crc;
}

/**
 * @brief Reads a little-endian 16-bit field.
 *
 * Combines two consecutive input bytes without alignment requirements.
 *
 * @param[in] input Two-byte field to read.
 * @return Decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t *input) { return input[0] | ((uint16_t)input[1] << 8); }

/**
 * @brief Reads a little-endian signed 32-bit field.
 *
 * Combines four consecutive input bytes and preserves the resulting two's-complement bit pattern.
 *
 * @param[in] input Four-byte field to read.
 * @return Decoded signed value.
 */
static int32_t read_i32(const uint8_t *input) {
    uint32_t value = (uint32_t)input[0] | (uint32_t)input[1] << 8 | (uint32_t)input[2] << 16 |
                     (uint32_t)input[3] << 24;
    return (int32_t)value;
}

/**
 * @brief Writes a little-endian 16-bit field.
 *
 * Stores the low byte before the high byte.
 *
 * @param[out] output Two-byte field to update.
 * @param[in] value Unsigned value to encode.
 */
static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Encodes one framed motor-link packet and its CRC-16/CCITT checksum.
 *
 * Writes the packet boundaries, type, payload, and checksum used by each SPI exchange with the
 * motor controller.
 *
 * @param[in] frame Packet type and eight-byte payload.
 * @param[out] output Thirteen-byte framed packet.
 */
void motor_live_frame_encode(const MotorLiveFrame *frame, uint8_t output[MOTOR_LIVE_FRAME_SIZE]) {
    output[0] = MOTOR_LIVE_FRAME_START;
    output[1] = frame->type;
    for (uint8_t index = 0; index < MOTOR_LIVE_PAYLOAD_SIZE; index++) {
        output[index + 2] = frame->payload[index];
    }
    write_u16(output + MOTOR_LIVE_CHECKSUM_OFFSET,
              checksum(output + MOTOR_LIVE_CHECKSUM_INPUT_OFFSET));
    output[12] = MOTOR_LIVE_FRAME_END;
}

/**
 * @brief Checks and decodes one complete motor-link packet.
 *
 * Validates both packet boundaries and the CRC before publishing the decoded type and payload.
 *
 * @param[in] input Thirteen-byte packet received from the motor controller.
 * @param[out] frame Decoded packet type and payload when the packet is valid.
 * @return The boundary or checksum result.
 */
MotorLiveFrameResult motor_live_frame_decode(const uint8_t input[MOTOR_LIVE_FRAME_SIZE],
                                             MotorLiveFrame *frame) {
    if (input[0] != MOTOR_LIVE_FRAME_START || input[12] != MOTOR_LIVE_FRAME_END) {
        return MOTOR_LIVE_FRAME_INVALID_BOUNDARY;
    }

    uint16_t expected = checksum(input + MOTOR_LIVE_CHECKSUM_INPUT_OFFSET);
    if (read_u16(input + MOTOR_LIVE_CHECKSUM_OFFSET) != expected) {
        return MOTOR_LIVE_FRAME_INVALID_CHECKSUM;
    }

    frame->type = input[1];
    for (uint8_t index = 0; index < MOTOR_LIVE_PAYLOAD_SIZE; index++) {
        frame->payload[index] = input[index + 2];
    }
    return MOTOR_LIVE_FRAME_VALID;
}

/**
 * @brief Decodes a current or replayed wheel-position packet.
 *
 * Accepts live and replay position packet types and expands their packed position, torque, and
 * auxiliary sensor fields.
 *
 * @param[in] frame Decoded motor-link packet.
 * @param[out] report Wheel position, measured torque, and auxiliary position fields.
 * @return True for position and replay packet types; otherwise false.
 */
bool motor_position_report_decode(const MotorLiveFrame *frame, MotorPositionReport *report) {
    if ((frame->type & ~MOTOR_LIVE_REPLAY_FLAG) != MOTOR_LIVE_POSITION_TYPE) {
        return false;
    }

    uint16_t auxiliary = read_u16(frame->payload + 6);
    report->replay = (frame->type & MOTOR_LIVE_REPLAY_FLAG) != 0;
    report->wheel_position = read_i32(frame->payload);
    report->motor_torque = read_u16(frame->payload + 4);
    report->auxiliary_negative = (auxiliary & MOTOR_POSITION_AUXILIARY_DIRECTION) != 0;
    report->auxiliary_position = (uint16_t)(auxiliary << 1);
    return true;
}

/**
 * @brief Builds the live force-output payload returned during the motor-link exchange.
 *
 * Combines the stored wheel center with the final primary and secondary force channels in a live
 * motor-controller packet.
 *
 * @param[in] center_position Stored wheel-center position.
 * @param[in] report Final direction and force magnitudes.
 * @param[out] frame Initialized live packet ready for framing.
 */
void motor_live_force_frame_init(int16_t center_position, const ForceOutputReport *report,
                                 MotorLiveFrame *frame) {
    frame->type = MOTOR_LIVE_POSITION_TYPE;
    write_u16(frame->payload, (uint16_t)center_position);
    force_output_report_encode(report, frame->payload + 2);
    frame->payload[7] = 0;
}
