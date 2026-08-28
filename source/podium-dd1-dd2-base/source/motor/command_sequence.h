#ifndef OPENTEC_BASE_MOTOR_COMMAND_SEQUENCE_H
#define OPENTEC_BASE_MOTOR_COMMAND_SEQUENCE_H

#include <stdint.h>

typedef enum {
    MOTOR_COMMAND_SEQUENCE_INVALID,
    MOTOR_COMMAND_SEQUENCE_ACKNOWLEDGED,
    MOTOR_COMMAND_SEQUENCE_PAYLOAD,
    MOTOR_COMMAND_SEQUENCE_RESEND,
    MOTOR_COMMAND_SEQUENCE_RESET,
    MOTOR_COMMAND_SEQUENCE_RETRY,
} MotorCommandSequenceEvent;

typedef struct {
    uint8_t transmit;
    uint8_t receive_previous;
    uint8_t receive_next;
} MotorCommandSequence;

void motor_command_sequence_init(MotorCommandSequence *sequence);
void motor_command_sequence_advance(MotorCommandSequence *sequence);
MotorCommandSequenceEvent motor_command_sequence_receive_header(MotorCommandSequence *sequence,
                                                                uint8_t header);
void motor_command_sequence_accept_payload(MotorCommandSequence *sequence, uint8_t header);

#endif
