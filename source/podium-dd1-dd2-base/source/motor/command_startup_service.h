#ifndef OPENTEC_BASE_MOTOR_COMMAND_STARTUP_SERVICE_H
#define OPENTEC_BASE_MOTOR_COMMAND_STARTUP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "motor/command_startup.h"
#include "transfer/command.h"

typedef enum {
    MOTOR_COMMAND_STARTUP_SERVICE_RUNNING,
    MOTOR_COMMAND_STARTUP_SERVICE_COMPLETE,
    MOTOR_COMMAND_STARTUP_SERVICE_FAILED,
} MotorCommandStartupServiceResult;

typedef enum {
    MOTOR_COMMAND_STARTUP_WRITE_NONE,
    MOTOR_COMMAND_STARTUP_WRITE_RESET,
    MOTOR_COMMAND_STARTUP_WRITE_REQUEST,
    MOTOR_COMMAND_STARTUP_WRITE_CONTROL,
} MotorCommandStartupWrite;

typedef struct {
    MotorCommandStartup startup;
    uint8_t length_record[MOTOR_COMMAND_MAILBOX_LENGTH_SIZE];
    uint8_t current_command;
    MotorCommandStartupWrite pending_write;
    bool status_read_pending;
    bool response_ready;
    bool failed;
} MotorCommandStartupService;

void motor_command_startup_service_init(MotorCommandStartupService *service);
MotorCommandStartupServiceResult
motor_command_startup_service_run(MotorCommandStartupService *service, MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport);

#endif
