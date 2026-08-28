#include <assert.h>
#include <stdint.h>

#include "motor/command_sequence.h"

static void test_initializes_and_advances_sequences(void) {
    MotorCommandSequence sequence;

    motor_command_sequence_init(&sequence);
    assert(sequence.transmit == 0);
    assert(sequence.receive_previous == 3);
    assert(sequence.receive_next == 0);

    motor_command_sequence_advance(&sequence);
    motor_command_sequence_advance(&sequence);
    motor_command_sequence_advance(&sequence);
    assert(sequence.transmit == 3);
    motor_command_sequence_advance(&sequence);
    assert(sequence.transmit == 0);
}

static void test_accepts_acknowledgements_and_requests_resend(void) {
    MotorCommandSequence sequence;
    motor_command_sequence_init(&sequence);

    assert(motor_command_sequence_receive_header(&sequence, 0x83) ==
           MOTOR_COMMAND_SEQUENCE_ACKNOWLEDGED);
    assert(motor_command_sequence_receive_header(&sequence, 0x82) == MOTOR_COMMAND_SEQUENCE_RESEND);
    assert(sequence.transmit == 3);
    assert(motor_command_sequence_receive_header(&sequence, 0x82) ==
           MOTOR_COMMAND_SEQUENCE_ACKNOWLEDGED);
}

static void test_applies_retry_and_reset_responses(void) {
    MotorCommandSequence sequence;
    motor_command_sequence_init(&sequence);

    assert(motor_command_sequence_receive_header(&sequence, 0xa2) == MOTOR_COMMAND_SEQUENCE_RETRY);
    assert(sequence.transmit == 2);
    assert(motor_command_sequence_receive_header(&sequence, 0x21) == MOTOR_COMMAND_SEQUENCE_RETRY);
    assert(sequence.transmit == 1);

    sequence.receive_previous = 1;
    sequence.receive_next = 2;
    assert(motor_command_sequence_receive_header(&sequence, 0xc3) == MOTOR_COMMAND_SEQUENCE_RESET);
    assert(sequence.transmit == 0);
    assert(sequence.receive_previous == 3);
    assert(sequence.receive_next == 0);
}

static void test_accepts_payload_sequence(void) {
    MotorCommandSequence sequence;
    motor_command_sequence_init(&sequence);

    assert(motor_command_sequence_receive_header(&sequence, 0x03) ==
           MOTOR_COMMAND_SEQUENCE_PAYLOAD);
    motor_command_sequence_accept_payload(&sequence, 0x03);
    assert(sequence.receive_previous == 0);
    assert(sequence.receive_next == 1);

    assert(motor_command_sequence_receive_header(&sequence, 0x0f) ==
           MOTOR_COMMAND_SEQUENCE_PAYLOAD);
    motor_command_sequence_accept_payload(&sequence, 0x0f);
    assert(sequence.receive_previous == 3);
    assert(sequence.receive_next == 0);
}

static void test_rejects_invalid_headers(void) {
    MotorCommandSequence sequence;
    motor_command_sequence_init(&sequence);

    assert(motor_command_sequence_receive_header(&sequence, 0x02) ==
           MOTOR_COMMAND_SEQUENCE_INVALID);
    assert(motor_command_sequence_receive_header(&sequence, 0x40) ==
           MOTOR_COMMAND_SEQUENCE_INVALID);
    assert(motor_command_sequence_receive_header(&sequence, 0x60) ==
           MOTOR_COMMAND_SEQUENCE_INVALID);
    assert(motor_command_sequence_receive_header(&sequence, 0xe0) ==
           MOTOR_COMMAND_SEQUENCE_INVALID);
}

int main(void) {
    test_initializes_and_advances_sequences();
    test_accepts_acknowledgements_and_requests_resend();
    test_applies_retry_and_reset_responses();
    test_accepts_payload_sequence();
    test_rejects_invalid_headers();
    return 0;
}
