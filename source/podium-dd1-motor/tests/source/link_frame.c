#include "motor/link_frame.h"

#include <assert.h>
#include <stdint.h>

static const uint8_t force_frame[MOTOR_LINK_FRAME_SIZE] = {
    0x7bU, 0x01U, 0x05U, 0x06U, 0x07U, 0x08U, 0xcdU, 0xabU, 0x68U, 0x24U, 0xfbU, 0xa4U, 0x7dU,
};

static void test_force_frame(void) {
    MotorLinkFrame frame;
    assert(motor_link_frame_decode_checked(force_frame, 0xa4fbU, &frame) == MOTOR_LINK_FRAME_VALID);
    MotorLinkForceCommand command;
    assert(motor_link_force_command_decode(&frame, &command));
    assert(command.center == 0x0605);
    assert(command.positive);
    assert(command.primary == 0xcd08U);
    assert(command.secondary == 0x68ab);

    MotorLinkStatusCommand status;
    assert(!motor_link_status_command_decode(&frame, &status));
}

static void test_position_frame(void) {
    const MotorLinkPositionReport report = {
        .position = 0x08070605,
        .torque = 0xabcdU,
        .drive_current = 0x2468,
    };
    uint8_t output[MOTOR_LINK_FRAME_SIZE];
    motor_link_position_frame_prepare(&report, output);
    motor_link_frame_checksum_write(output, 0xa4fbU);
    for (uint8_t index = 0U; index < MOTOR_LINK_FRAME_SIZE; ++index) {
        assert(output[index] == force_frame[index]);
    }

    MotorLinkPositionReport replay = report;
    replay.replay = true;
    replay.positive = true;
    replay.drive_current = -1234;
    motor_link_position_frame_prepare(&replay, output);
    assert(output[1] == 0x81U);
    assert(output[8] == 0xd2U);
    assert(output[9] == 0x84U);
}

static void test_status_frame(void) {
    const uint8_t input[MOTOR_LINK_FRAME_SIZE] = {
        0x7bU, 0x02U, 0x33U, 0x21U, 0x08U, 0x80U, 0U, 0U, 0U, 0U, 0x34U, 0x12U, 0x7dU,
    };
    MotorLinkFrame frame;
    assert(motor_link_frame_decode_checked(input, 0x1234U, &frame) == MOTOR_LINK_FRAME_VALID);
    MotorLinkStatusCommand command;
    assert(motor_link_status_command_decode(&frame, &command));
    assert(command.status == 0x33U);
    assert(command.command[0] == 0x21U);
    assert(command.command[1] == 0x08U);
    assert(command.command[2] == 0x80U);
    assert(command.command[6] == 0U);
}

static void test_rejects_invalid_frame(void) {
    uint8_t input[MOTOR_LINK_FRAME_SIZE];
    for (uint8_t index = 0U; index < MOTOR_LINK_FRAME_SIZE; ++index) {
        input[index] = force_frame[index];
    }
    input[0] = 0U;
    MotorLinkFrame frame;
    assert(motor_link_frame_decode_checked(input, 0xa4fbU, &frame) ==
           MOTOR_LINK_FRAME_INVALID_BOUNDARY);
    input[0] = 0x7bU;
    assert(motor_link_frame_decode_checked(input, 0U, &frame) == MOTOR_LINK_FRAME_INVALID_CHECKSUM);
}

int main(void) {
    test_force_frame();
    test_position_frame();
    test_status_frame();
    test_rejects_invalid_frame();
    return 0;
}
