#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_receiver.h"

static const uint8_t acknowledgement[] = {0x83, 0, 0, 0xe3, 0x88};
static const uint8_t resend[] = {0x82, 0, 0, 0xb9, 0x54};
static const uint8_t retry[] = {0xa2, 0, 0, 0xba, 0x6f};
static const uint8_t reset[] = {0xc0, 0, 0, 0x0a, 0x9a};
static const uint8_t message[] = {0x03, 0x00, 0x04, 0x00, 0x11, 0x22, 0x33, 0x49, 0xac};
static const uint8_t first_fragment[] = {0x03, 0x00, 0x04, 0x01, 0x41, 0x42, 0x43, 0xc0, 0x26};
static const uint8_t continuation_fragment[] = {0x03, 0x00, 0x03, 0x02, 0x44, 0x45, 0xa8, 0xa7};
static const uint8_t final_fragment[] = {0x03, 0x00, 0x02, 0x04, 0x46, 0xe8, 0x26};

static void test_handles_control_packets(void) {
    uint8_t assembly[16];
    MotorCommandReceiver receiver;
    motor_command_receiver_init(&receiver, assembly, sizeof(assembly));

    assert(
        motor_command_receiver_accept(&receiver, acknowledgement, sizeof(acknowledgement)).result ==
        MOTOR_COMMAND_RECEIVE_ACKNOWLEDGED);
    assert(motor_command_receiver_accept(&receiver, resend, sizeof(resend)).result ==
           MOTOR_COMMAND_RECEIVE_RESEND);
    assert(receiver.sequence.transmit == 3);
    assert(motor_command_receiver_accept(&receiver, retry, sizeof(retry)).result ==
           MOTOR_COMMAND_RECEIVE_RETRY);
    assert(receiver.sequence.transmit == 2);
    assert(motor_command_receiver_accept(&receiver, reset, sizeof(reset)).result ==
           MOTOR_COMMAND_RECEIVE_RESET);
    assert(receiver.sequence.transmit == 0);
}

static void test_exposes_unfragmented_payload(void) {
    static const uint8_t expected[] = {0x11, 0x22, 0x33};
    uint8_t assembly[16];
    MotorCommandReceiver receiver;
    motor_command_receiver_init(&receiver, assembly, sizeof(assembly));

    MotorCommandReceiveEvent event =
        motor_command_receiver_accept(&receiver, message, sizeof(message));

    assert(event.result == MOTOR_COMMAND_RECEIVE_MESSAGE);
    assert(event.payload_length == sizeof(expected));
    assert(memcmp(event.payload, expected, sizeof(expected)) == 0);
    assert(receiver.sequence.receive_previous == 0);
    assert(receiver.sequence.receive_next == 1);
}

static void test_assembles_fragmented_payload(void) {
    static const uint8_t expected[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    uint8_t assembly[16];
    MotorCommandReceiver receiver;
    motor_command_receiver_init(&receiver, assembly, sizeof(assembly));

    assert(
        motor_command_receiver_accept(&receiver, first_fragment, sizeof(first_fragment)).result ==
        MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING);
    assert(motor_command_receiver_accept(&receiver, continuation_fragment,
                                         sizeof(continuation_fragment))
               .result == MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING);
    MotorCommandReceiveEvent event =
        motor_command_receiver_accept(&receiver, final_fragment, sizeof(final_fragment));

    assert(event.result == MOTOR_COMMAND_RECEIVE_MESSAGE);
    assert(event.payload_length == sizeof(expected));
    assert(memcmp(event.payload, expected, sizeof(expected)) == 0);
}

static void test_rejects_malformed_packets(void) {
    uint8_t assembly[16];
    uint8_t invalid[sizeof(message)];
    MotorCommandReceiver receiver;
    motor_command_receiver_init(&receiver, assembly, sizeof(assembly));

    memcpy(invalid, message, sizeof(invalid));
    invalid[2]--;
    assert(motor_command_receiver_accept(&receiver, invalid, sizeof(invalid)).result ==
           MOTOR_COMMAND_RECEIVE_INVALID);
    memcpy(invalid, message, sizeof(invalid));
    invalid[sizeof(invalid) - 1] ^= 1;
    assert(motor_command_receiver_accept(&receiver, invalid, sizeof(invalid)).result ==
           MOTOR_COMMAND_RECEIVE_INVALID);
}

int main(void) {
    test_handles_control_packets();
    test_exposes_unfragmented_payload();
    test_assembles_fragmented_payload();
    test_rejects_malformed_packets();
    return 0;
}
