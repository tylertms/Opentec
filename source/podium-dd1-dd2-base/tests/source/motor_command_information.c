#include <assert.h>
#include <stdint.h>

#include "motor/command_information.h"
#include "motor/command_message.h"

static MotorCommandMessage information(uint8_t selector, const uint8_t *data, uint16_t length) {
    return (MotorCommandMessage){
        .kind = MOTOR_COMMAND_MESSAGE_INFORMATION,
        .command = 0x85,
        .selector = selector,
        .data = data,
        .data_length = length,
    };
}

static void test_stores_scalar_selectors(void) {
    static const uint8_t word[] = {0x12, 0x34};
    static const uint8_t byte[] = {0x56};
    MotorCommandInformation state = {0};
    MotorCommandMessage message = information(1, word, sizeof(word));

    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    assert(state.selector_1 == 0x1234);
    message.selector = 3;
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    assert(state.selector_3 == 0x1234);
    message.selector = 4;
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    assert(state.selector_4 == 0x1234);
    message = information(5, byte, sizeof(byte));
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    assert(state.selector_5 == 0x56);
    message = information(6, word, sizeof(word));
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    assert(state.selector_6 == 0x1234);
}

static void test_stores_block_selectors(void) {
    uint8_t selector_7[16];
    uint8_t selector_8[4];
    uint8_t selector_9[50];
    MotorCommandInformation state = {0};

    for (uint8_t index = 0; index < sizeof(selector_7); index++) {
        selector_7[index] = index;
    }
    for (uint8_t index = 0; index < sizeof(selector_8); index++) {
        selector_8[index] = index + 16;
    }
    for (uint8_t index = 0; index < sizeof(selector_9); index++) {
        selector_9[index] = index + 20;
    }

    MotorCommandMessage message = information(7, selector_7, sizeof(selector_7));
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    message = information(8, selector_8, sizeof(selector_8));
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);
    message = information(9, selector_9, sizeof(selector_9));
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_STORED);

    for (uint8_t index = 0; index < sizeof(selector_7); index++) {
        assert(state.selector_7[index] == selector_7[index]);
    }
    for (uint8_t index = 0; index < sizeof(selector_8); index++) {
        assert(state.selector_8[index] == selector_8[index]);
    }
    for (uint8_t index = 0; index < sizeof(selector_9); index++) {
        assert(state.selector_9[index] == selector_9[index]);
    }
}

static void test_forwards_selector_two(void) {
    static const uint8_t data[20] = {0};
    MotorCommandInformation state = {0};
    MotorCommandMessage message = information(2, data, sizeof(data));

    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_FORWARD);
}

static void test_rejects_invalid_responses(void) {
    static const uint8_t data[2] = {0};
    MotorCommandInformation state = {0};
    MotorCommandMessage message = information(1, data, 1);

    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_INVALID);
    message = information(10, data, sizeof(data));
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_INVALID);
    message.kind = MOTOR_COMMAND_MESSAGE_CALIBRATION;
    assert(motor_command_information_apply(&state, &message) == MOTOR_COMMAND_INFORMATION_INVALID);
}

int main(void) {
    test_stores_scalar_selectors();
    test_stores_block_selectors();
    test_forwards_selector_two();
    test_rejects_invalid_responses();
    return 0;
}
