#ifndef OPENTEC_BASE_MOTOR_COMMAND_STARTUP_H
#define OPENTEC_BASE_MOTOR_COMMAND_STARTUP_H

#include <stdbool.h>
#include <stdint.h>

enum {
    MOTOR_COMMAND_STARTUP_OWNER = 0x20,
    MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND = 0xfe,
    MOTOR_COMMAND_STARTUP_DIGEST_COMMAND = 7,
    MOTOR_COMMAND_STARTUP_INFO_COMMAND = 5,
    MOTOR_COMMAND_STARTUP_FIRST_INFO_SELECTOR = 3,
    MOTOR_COMMAND_STARTUP_SECOND_INFO_SELECTOR = 4,
};

typedef enum {
    MOTOR_COMMAND_STARTUP_RESET,
    MOTOR_COMMAND_STARTUP_CLAIM,
    MOTOR_COMMAND_STARTUP_READ_LENGTH,
    MOTOR_COMMAND_STARTUP_WAIT_LENGTH,
    MOTOR_COMMAND_STARTUP_WAIT_RESET,
    MOTOR_COMMAND_STARTUP_WAIT_DIGEST,
    MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO,
    MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO,
    MOTOR_COMMAND_STARTUP_CONFIRM,
    MOTOR_COMMAND_STARTUP_FINISH,
    MOTOR_COMMAND_STARTUP_DONE,
} MotorCommandStartupPhase;

typedef enum {
    MOTOR_COMMAND_STARTUP_ACTION_NONE,
    MOTOR_COMMAND_STARTUP_ACTION_CLAIM,
    MOTOR_COMMAND_STARTUP_ACTION_RELEASE,
    MOTOR_COMMAND_STARTUP_ACTION_READ_LENGTH,
    MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND,
} MotorCommandStartupActionType;

typedef struct {
    MotorCommandStartupActionType type;
    uint8_t command;
    uint8_t selector;
} MotorCommandStartupAction;

typedef struct {
    uint8_t command;
    bool length_read_pending;
    bool response_ready;
    bool restart;
} MotorCommandStartupInput;

typedef struct {
    MotorCommandStartupPhase phase;
    bool active;
    bool complete;
} MotorCommandStartup;

void motor_command_startup_init(MotorCommandStartup *startup);
MotorCommandStartupAction motor_command_startup_run(MotorCommandStartup *startup,
                                                    const MotorCommandStartupInput *input);

#endif
