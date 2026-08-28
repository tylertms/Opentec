#ifndef OPENTEC_BASE_MOTOR_COMMAND_INFORMATION_H
#define OPENTEC_BASE_MOTOR_COMMAND_INFORMATION_H

#include <stdint.h>

#include "motor/command_message.h"

typedef enum {
    MOTOR_COMMAND_INFORMATION_INVALID,
    MOTOR_COMMAND_INFORMATION_STORED,
    MOTOR_COMMAND_INFORMATION_FORWARD,
} MotorCommandInformationResult;

typedef struct {
    uint16_t selector_1;
    uint16_t selector_3;
    uint16_t selector_4;
    uint8_t selector_5;
    uint16_t selector_6;
    uint8_t selector_7[16];
    uint8_t selector_8[4];
    uint8_t selector_9[50];
} MotorCommandInformation;

MotorCommandInformationResult motor_command_information_apply(MotorCommandInformation *state,
                                                              const MotorCommandMessage *message);

#endif
