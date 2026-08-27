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

static uint16_t crc16_shift(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) != 0 ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint16_t checksum(const uint8_t *input) {
    uint16_t crc = 0;
    for (uint8_t index = 0; index < MOTOR_LIVE_CHECKSUM_INPUT_SIZE; index++) {
        crc = crc16_shift(crc, input[index]);
    }
    crc = crc16_shift(crc, 0);
    return crc16_shift(crc, 0);
}

static uint16_t read_u16(const uint8_t *input) { return input[0] | ((uint16_t)input[1] << 8); }

static int32_t read_i32(const uint8_t *input) {
    uint32_t value = (uint32_t)input[0] | (uint32_t)input[1] << 8 | (uint32_t)input[2] << 16 |
                     (uint32_t)input[3] << 24;
    return (int32_t)value;
}

static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

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

bool motor_position_report_decode(const MotorLiveFrame *frame, MotorPositionReport *report) {
    if ((frame->type & ~MOTOR_LIVE_REPLAY_FLAG) != MOTOR_LIVE_POSITION_TYPE) {
        return false;
    }

    uint16_t auxiliary = read_u16(frame->payload + 6);
    report->replay = (frame->type & MOTOR_LIVE_REPLAY_FLAG) != 0;
    report->wheel_position = read_i32(frame->payload);
    report->motor_torque = read_u16(frame->payload + 4);
    report->auxiliary_negative = (auxiliary & MOTOR_POSITION_AUXILIARY_DIRECTION) != 0;
    report->auxiliary_magnitude = auxiliary & ~MOTOR_POSITION_AUXILIARY_DIRECTION;
    return true;
}

void motor_force_frame_init(int16_t center_position, const ForceOutputCommand *command,
                            uint16_t secondary_magnitude, MotorLiveFrame *frame) {
    frame->type = MOTOR_LIVE_POSITION_TYPE;
    write_u16(frame->payload, (uint16_t)center_position);
    force_output_report_encode(command, secondary_magnitude, frame->payload + 2);
    frame->payload[7] = 0;
}
