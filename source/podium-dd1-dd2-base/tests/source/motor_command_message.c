#include <assert.h>
#include <stdint.h>

#include "motor/command_message.h"

static void test_decodes_information_message(void) {
    static const uint8_t payload[] = {0x85, 0, 3, 0, 2, 0x12, 0x34};
    MotorCommandMessage message;

    assert(motor_command_message_decode(payload, sizeof(payload), &message));
    assert(message.kind == MOTOR_COMMAND_MESSAGE_INFORMATION);
    assert(message.command == 0x85);
    assert(message.selector == 3);
    assert(message.data_length == 2);
    assert(message.data[0] == 0x12);
    assert(message.data[1] == 0x34);
}

static void test_decodes_calibration_message(void) {
    static const uint8_t payload[] = {
        0x87, 0, 0, 0, 20, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0,
    };
    MotorCommandMessage message;

    assert(motor_command_message_decode(payload, sizeof(payload), &message));
    assert(message.kind == MOTOR_COMMAND_MESSAGE_CALIBRATION);
    assert(message.data_length == 20);
    assert(message.data[15] == 15);
}

static void test_decodes_vendor_messages(void) {
    static const uint8_t continuation[] = {0xc0, 1, 2};
    static const uint8_t final[] = {0xc1, 3, 4};
    MotorCommandMessage message;

    assert(motor_command_message_decode(continuation, sizeof(continuation), &message));
    assert(message.kind == MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION);
    assert(message.data == continuation);
    assert(message.data_length == sizeof(continuation));
    assert(motor_command_message_decode(final, sizeof(final), &message));
    assert(message.kind == MOTOR_COMMAND_MESSAGE_VENDOR_FINAL);
    assert(message.data == final);
}

static void test_rejects_unsupported_or_truncated_messages(void) {
    static const uint8_t unsupported[] = {0x84};
    static const uint8_t short_information[] = {0x85, 0, 3, 0};
    static const uint8_t truncated_data[] = {0x85, 0, 3, 0, 2, 0x12};
    MotorCommandMessage message;

    assert(!motor_command_message_decode(unsupported, sizeof(unsupported), &message));
    assert(!motor_command_message_decode(short_information, sizeof(short_information), &message));
    assert(!motor_command_message_decode(truncated_data, sizeof(truncated_data), &message));
}

int main(void) {
    test_decodes_information_message();
    test_decodes_calibration_message();
    test_decodes_vendor_messages();
    test_rejects_unsupported_or_truncated_messages();
    return 0;
}
