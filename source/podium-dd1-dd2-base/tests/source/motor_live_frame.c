#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/live_frame.h"

static const uint8_t position_frame[MOTOR_LIVE_FRAME_SIZE] = {
    0x7b, 0x01, 0x05, 0x06, 0x07, 0x08, 0xcd, 0xab, 0x68, 0x24, 0xfb, 0xa4, 0x7d,
};

static void test_decode_position(void) {
    MotorLiveFrame frame;
    MotorPositionReport report;

    assert(motor_live_frame_decode(position_frame, &frame) == MOTOR_LIVE_FRAME_VALID);
    assert(motor_position_report_decode(&frame, &report));
    assert(!report.replay);
    assert(report.wheel_position == 0x08070605);
    assert(report.motor_torque == 0xabcd);
    assert(!report.auxiliary_negative);
    assert(report.auxiliary_position == 0x48d0);
}

static void test_decode_replay(void) {
    const uint8_t encoded[MOTOR_LIVE_FRAME_SIZE] = {
        0x7b, 0x81, 0xde, 0xad, 0xbe, 0xef, 0x68, 0x24, 0xe0, 0xac, 0x6b, 0x04, 0x7d,
    };
    MotorLiveFrame frame;
    MotorPositionReport report;

    assert(motor_live_frame_decode(encoded, &frame) == MOTOR_LIVE_FRAME_VALID);
    assert(motor_position_report_decode(&frame, &report));
    assert(report.replay);
    assert(report.wheel_position == (int32_t)0xefbeaddeu);
    assert(report.motor_torque == 0x2468);
    assert(report.auxiliary_negative);
    assert(report.auxiliary_position == 0x59c0);
}

static void test_encode_force(void) {
    const ForceOutputReport report = {
        .positive_direction = false,
        .primary_magnitude = 0x1234,
        .secondary_magnitude = 0x5678,
    };
    MotorLiveFrame frame;
    MotorLiveFrame decoded;
    uint8_t encoded[MOTOR_LIVE_FRAME_SIZE];

    motor_live_force_frame_init(-321, &report, &frame);
    motor_live_frame_encode(&frame, encoded);

    assert(encoded[0] == MOTOR_LIVE_FRAME_START);
    assert(encoded[1] == MOTOR_LIVE_POSITION_TYPE);
    assert(encoded[2] == 0xbf);
    assert(encoded[3] == 0xfe);
    assert(encoded[4] == 0);
    assert(encoded[5] == 0x34);
    assert(encoded[6] == 0x12);
    assert(encoded[7] == 0x78);
    assert(encoded[8] == 0x56);
    assert(encoded[9] == 0);
    assert(encoded[12] == MOTOR_LIVE_FRAME_END);
    assert(motor_live_frame_decode(encoded, &decoded) == MOTOR_LIVE_FRAME_VALID);
    assert(memcmp(&decoded, &frame, sizeof(frame)) == 0);
}

static void test_rejects_invalid_frames(void) {
    uint8_t input[MOTOR_LIVE_FRAME_SIZE];
    MotorLiveFrame frame = {0};

    memcpy(input, position_frame, sizeof(input));
    input[0] = 0;
    assert(motor_live_frame_decode(input, &frame) == MOTOR_LIVE_FRAME_INVALID_BOUNDARY);

    memcpy(input, position_frame, sizeof(input));
    input[12] = 0;
    assert(motor_live_frame_decode(input, &frame) == MOTOR_LIVE_FRAME_INVALID_BOUNDARY);

    memcpy(input, position_frame, sizeof(input));
    input[2] ^= 1;
    assert(motor_live_frame_decode(input, &frame) == MOTOR_LIVE_FRAME_INVALID_CHECKSUM);

    frame.type = MOTOR_LIVE_STATUS_TYPE;
    MotorPositionReport report;
    assert(!motor_position_report_decode(&frame, &report));
}

int main(void) {
    test_decode_position();
    test_decode_replay();
    test_encode_force();
    test_rejects_invalid_frames();
    return 0;
}
