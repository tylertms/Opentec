#ifndef OPENTEC_BASE_MOTOR_COMMAND_RECEIVER_H
#define OPENTEC_BASE_MOTOR_COMMAND_RECEIVER_H

#include <stdint.h>

#include "motor/command_fragment.h"
#include "motor/command_sequence.h"

typedef enum {
    MOTOR_COMMAND_RECEIVE_INVALID,
    MOTOR_COMMAND_RECEIVE_ACKNOWLEDGED,
    MOTOR_COMMAND_RECEIVE_RESEND,
    MOTOR_COMMAND_RECEIVE_RETRY,
    MOTOR_COMMAND_RECEIVE_RESET,
    MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING,
    MOTOR_COMMAND_RECEIVE_MESSAGE,
    MOTOR_COMMAND_RECEIVE_IGNORED,
} MotorCommandReceiveResult;

typedef struct {
    MotorCommandReceiveResult result;
    const uint8_t *payload;
    uint16_t payload_length;
} MotorCommandReceiveEvent;

typedef struct {
    MotorCommandSequence sequence;
    MotorCommandFragment fragment;
} MotorCommandReceiver;

void motor_command_receiver_init(MotorCommandReceiver *receiver, uint8_t *assembly,
                                 uint16_t assembly_capacity);
MotorCommandReceiveEvent motor_command_receiver_accept(MotorCommandReceiver *receiver,
                                                       const uint8_t *packet, uint16_t length);

#endif
