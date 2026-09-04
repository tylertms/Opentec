#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/live_frame.h"

static const uint8_t position_frame[MOTOR_LIVE_FRAME_SIZE] = {
    0x7b, 0x01, 0x05, 0x06, 0x07, 0x08, 0xcd, 0xab, 0x68, 0x24, 0xfe, 0xe9, 0x7d,
};

static void test_decode_position(void) {
    MotorLiveFrame frame;
    MotorPositionReport report;

    assert(motor_live_frame_decode(position_frame, &frame) == MOTOR_LIVE_FRAME_VALID);
    assert(motor_position_report_decode(&frame, &report));
    assert(report.wheel_position == 0x08070605);
    assert(report.motor_torque == 0xabcd);
    assert(!report.auxiliary_negative);
    assert(report.auxiliary_position == 0x48d0);

    uint8_t reencoded[MOTOR_LIVE_FRAME_SIZE];
    motor_live_frame_encode(&frame, reencoded);
    assert(memcmp(reencoded, position_frame, sizeof(reencoded)) == 0);
}

static void test_replay_is_not_a_position_report(void) {
    const uint8_t encoded[MOTOR_LIVE_FRAME_SIZE] = {
        0x7b, 0x81, 0xde, 0xad, 0xbe, 0xef, 0x68, 0x24, 0xe0, 0xac, 0x59, 0x14, 0x7d,
    };
    MotorLiveFrame frame;
    MotorPositionReport report = {
        .wheel_position = 123,
        .motor_torque = 456,
        .auxiliary_negative = true,
        .auxiliary_position = 789,
    };
    MotorPositionReport before = report;

    assert(motor_live_frame_decode(encoded, &frame) == MOTOR_LIVE_FRAME_VALID);
    assert(motor_live_frame_requests_replay(&frame));
    assert(!motor_position_report_decode(&frame, &report));
    assert(memcmp(&report, &before, sizeof(report)) == 0);

    const uint8_t non_replay_types[] = {0x00, MOTOR_LIVE_POSITION_TYPE, MOTOR_LIVE_REPLAY_FLAG,
                                        MOTOR_LIVE_STATUS_TYPE,
                                        MOTOR_LIVE_STATUS_TYPE | MOTOR_LIVE_REPLAY_FLAG};
    for (size_t index = 0; index < sizeof(non_replay_types); index++) {
        frame.type = non_replay_types[index];
        assert(!motor_live_frame_requests_replay(&frame));
    }
    assert(!motor_live_frame_requests_replay(NULL));
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
    assert(encoded[10] == 0x3b);
    assert(encoded[11] == 0x32);
    assert(encoded[12] == MOTOR_LIVE_FRAME_END);
    assert(motor_live_frame_decode(encoded, &decoded) == MOTOR_LIVE_FRAME_VALID);
    assert(memcmp(&decoded, &frame, sizeof(frame)) == 0);
}

static void test_inhibit_primary_preserves_replay_frame_state(void) {
    MotorLiveFrame frame = {
        .type = MOTOR_LIVE_POSITION_TYPE | MOTOR_LIVE_REPLAY_FLAG,
        .payload = {0x11, 0x22, 1, 0x34, 0x12, 0x78, 0x56, 0x99},
    };

    motor_live_frame_inhibit_primary(&frame);

    assert(frame.payload[0] == 0x11);
    assert(frame.payload[1] == 0x22);
    assert(frame.payload[2] == 1);
    assert(frame.payload[3] == 0);
    assert(frame.payload[4] == 0);
    assert(frame.payload[5] == 0x78);
    assert(frame.payload[6] == 0x56);
    assert(frame.payload[7] == 0x99);

    frame.type = MOTOR_LIVE_STATUS_TYPE;
    frame.payload[3] = 0x34;
    frame.payload[4] = 0x12;
    motor_live_frame_inhibit_primary(&frame);
    assert(frame.payload[3] == 0x34);
    assert(frame.payload[4] == 0x12);
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
    test_replay_is_not_a_position_report();
    test_encode_force();
    test_inhibit_primary_preserves_replay_frame_state();
    test_rejects_invalid_frames();
    return 0;
}
