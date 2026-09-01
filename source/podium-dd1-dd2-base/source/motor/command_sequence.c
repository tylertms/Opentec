#include "motor/command_sequence.h"

#include <stdint.h>

/**
 * @brief Internal motor-command sequence encoding constants.
 */
enum {
    MOTOR_COMMAND_SEQUENCE_MASK = 3,             /**< Mask for a two-bit sequence number. */
    MOTOR_COMMAND_SEQUENCE_CURRENT_SHIFT = 2,    /**< Shift for the current sequence field. */
    MOTOR_COMMAND_SEQUENCE_MODE_MASK = 0x60,     /**< Mask for the two-bit header mode. */
    MOTOR_COMMAND_SEQUENCE_RETRY_MODE = 0x20,    /**< Header mode requesting a retry. */
    MOTOR_COMMAND_SEQUENCE_RESET_MODE = 0x40,    /**< Header mode requesting a sequence reset. */
    MOTOR_COMMAND_SEQUENCE_RESPONSE_FLAG = 0x80, /**< Header flag identifying a response. */
};

void motor_command_sequence_init(MotorCommandSequence *sequence) {
    sequence->transmit = 0;
    sequence->receive_previous = 3;
    sequence->receive_next = 0;
}

void motor_command_sequence_advance(MotorCommandSequence *sequence) {
    sequence->transmit = (sequence->transmit + 1) & MOTOR_COMMAND_SEQUENCE_MASK;
}

MotorCommandSequenceEvent motor_command_sequence_receive_header(MotorCommandSequence *sequence,
                                                                uint8_t header) {
    uint8_t mode = header & MOTOR_COMMAND_SEQUENCE_MODE_MASK;
    uint8_t adjacent = header & MOTOR_COMMAND_SEQUENCE_MASK;
    if (mode == MOTOR_COMMAND_SEQUENCE_RETRY_MODE) {
        sequence->transmit = adjacent;
        return MOTOR_COMMAND_SEQUENCE_RETRY;
    }
    if ((header & MOTOR_COMMAND_SEQUENCE_RESPONSE_FLAG) != 0) {
        if (mode == MOTOR_COMMAND_SEQUENCE_RESET_MODE) {
            motor_command_sequence_init(sequence);
            return MOTOR_COMMAND_SEQUENCE_RESET;
        }
        if (mode != 0) {
            return MOTOR_COMMAND_SEQUENCE_INVALID;
        }
        uint8_t expected =
            sequence->transmit == 0 ? MOTOR_COMMAND_SEQUENCE_MASK : sequence->transmit - 1;
        if (adjacent == expected) {
            return MOTOR_COMMAND_SEQUENCE_ACKNOWLEDGED;
        }
        sequence->transmit = expected;
        return MOTOR_COMMAND_SEQUENCE_RESEND;
    }
    if (mode != 0) {
        return MOTOR_COMMAND_SEQUENCE_INVALID;
    }
    uint8_t expected =
        sequence->transmit == 0 ? MOTOR_COMMAND_SEQUENCE_MASK : sequence->transmit - 1;
    return adjacent == expected ? MOTOR_COMMAND_SEQUENCE_PAYLOAD : MOTOR_COMMAND_SEQUENCE_INVALID;
}

void motor_command_sequence_accept_payload(MotorCommandSequence *sequence, uint8_t header) {
    sequence->receive_previous =
        (header >> MOTOR_COMMAND_SEQUENCE_CURRENT_SHIFT) & MOTOR_COMMAND_SEQUENCE_MASK;
    sequence->receive_next = (sequence->receive_previous + 1) & MOTOR_COMMAND_SEQUENCE_MASK;
}
