#ifndef OPENTEC_BASE_MOTOR_COMMAND_APPLICATION_H
#define OPENTEC_BASE_MOTOR_COMMAND_APPLICATION_H

#include <stdint.h>

#include "motor/command_digest.h"
#include "motor/command_information.h"
#include "motor/command_message.h"

typedef enum {
    MOTOR_COMMAND_APPLICATION_INVALID,
    MOTOR_COMMAND_APPLICATION_INFORMATION,
    MOTOR_COMMAND_APPLICATION_CALIBRATION,
    MOTOR_COMMAND_APPLICATION_FORWARD,
} MotorCommandApplicationResult;

typedef struct {
    MotorCommandApplicationResult result;
    const uint8_t *forward_data;
    uint16_t forward_length;
} MotorCommandApplicationEvent;

typedef struct {
    MotorCommandInformation information;
    uint8_t digest[MOTOR_COMMAND_DIGEST_SIZE];
} MotorCommandApplication;

void motor_command_application_init(MotorCommandApplication *application);
MotorCommandApplicationEvent motor_command_application_apply(MotorCommandApplication *application,
                                                             const MotorCommandMessage *message);

#endif
