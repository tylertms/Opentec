#ifndef OPENTEC_BASE_MOTOR_COMMAND_MESSAGE_H
#define OPENTEC_BASE_MOTOR_COMMAND_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_COMMAND_MESSAGE_INFORMATION,
    MOTOR_COMMAND_MESSAGE_CALIBRATION,
    MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION,
    MOTOR_COMMAND_MESSAGE_VENDOR_FINAL,
} MotorCommandMessageKind;

typedef struct {
    MotorCommandMessageKind kind;
    uint8_t command;
    uint8_t selector;
    const uint8_t *payload;
    uint16_t payload_length;
    const uint8_t *data;
    uint16_t data_length;
} MotorCommandMessage;

bool motor_command_message_decode(const uint8_t *payload, uint16_t length,
                                  MotorCommandMessage *message);

#endif
