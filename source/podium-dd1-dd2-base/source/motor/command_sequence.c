#include "motor/command_sequence.h"

#include <stdint.h>

enum {
    MOTOR_COMMAND_SEQUENCE_MASK = 3,
    MOTOR_COMMAND_SEQUENCE_CURRENT_SHIFT = 2,
    MOTOR_COMMAND_SEQUENCE_MODE_MASK = 0x60,
    MOTOR_COMMAND_SEQUENCE_RETRY_MODE = 0x20,
    MOTOR_COMMAND_SEQUENCE_RESET_MODE = 0x40,
    MOTOR_COMMAND_SEQUENCE_RESPONSE_FLAG = 0x80,
};

/**
 * @brief Initializes motor-command sequence tracking.
 *
 * Starts transmitted packets at sequence zero and initializes the received previous and next
 * sequence values to three and zero.
 *
 * @param[out] sequence Sequence state to initialize.
 */
void motor_command_sequence_init(MotorCommandSequence *sequence) {
    sequence->transmit = 0;
    sequence->receive_previous = 3;
    sequence->receive_next = 0;
}

/**
 * @brief Advances the transmitted motor-command sequence.
 *
 * Increments the two-bit transmitted sequence and wraps from three to zero.
 *
 * @param[in,out] sequence Sequence state to advance.
 */
void motor_command_sequence_advance(MotorCommandSequence *sequence) {
    sequence->transmit = (sequence->transmit + 1) & MOTOR_COMMAND_SEQUENCE_MASK;
}

/**
 * @brief Classifies a received motor-command header.
 *
 * Matches acknowledgement and payload headers against the sequence preceding the current
 * transmitted value. Retry headers select the requested transmitted sequence, reset responses
 * restore all sequence values, and mismatched acknowledgements request a resend from the expected
 * preceding sequence.
 *
 * @param[in,out] sequence Sequence state updated by retry, reset, and resend headers.
 * @param[in] header Received packet header byte.
 * @return Header event selected by its response flag, mode, and adjacent sequence.
 */
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

/**
 * @brief Accepts a complete motor-command payload sequence.
 *
 * Stores header bits two and three as the received previous sequence and derives the wrapped next
 * sequence used by subsequent outgoing packets.
 *
 * @param[in,out] sequence Sequence state to update.
 * @param[in] header Header from the accepted complete payload packet.
 */
void motor_command_sequence_accept_payload(MotorCommandSequence *sequence, uint8_t header) {
    sequence->receive_previous =
        (header >> MOTOR_COMMAND_SEQUENCE_CURRENT_SHIFT) & MOTOR_COMMAND_SEQUENCE_MASK;
    sequence->receive_next = (sequence->receive_previous + 1) & MOTOR_COMMAND_SEQUENCE_MASK;
}
