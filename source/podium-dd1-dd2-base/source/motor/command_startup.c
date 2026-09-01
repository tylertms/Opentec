#include "motor/command_startup.h"

#include <stdbool.h>
#include <stdint.h>

void motor_command_startup_init(MotorCommandStartup *startup) {
    startup->phase = MOTOR_COMMAND_STARTUP_RESET;
    startup->active = false;
    startup->complete = false;
}

MotorCommandStartupAction motor_command_startup_run(MotorCommandStartup *startup,
                                                    CommandTransport *transport,
                                                    MotorCommandStartupInput input) {
    MotorCommandStartupAction action = {.type = MOTOR_COMMAND_STARTUP_ACTION_NONE};
    if (input.restart) {
        motor_command_startup_init(startup);
    }

    switch (startup->phase) {
    case MOTOR_COMMAND_STARTUP_RESET:
        command_transport_release(transport, MOTOR_COMMAND_STARTUP_OWNER);
        startup->phase = MOTOR_COMMAND_STARTUP_CLAIM;
        break;
    case MOTOR_COMMAND_STARTUP_CLAIM:
        command_transport_claim(transport, MOTOR_COMMAND_STARTUP_OWNER);
        startup->active = true;
        startup->phase = MOTOR_COMMAND_STARTUP_READ_STATUS;
        break;
    case MOTOR_COMMAND_STARTUP_READ_STATUS:
        startup->phase = MOTOR_COMMAND_STARTUP_WAIT_STATUS;
        action.type = MOTOR_COMMAND_STARTUP_ACTION_READ_STATUS;
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_STATUS:
        if (!input.status_read_pending) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_RESET;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_RESET:
        if (input.command == 0) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_DIGEST;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_DIGEST_COMMAND;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_DIGEST:
        if (input.response_ready) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_INFO_COMMAND;
            action.selector = MOTOR_COMMAND_STARTUP_FIRST_INFO_SELECTOR;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO:
        if (input.response_ready) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_INFO_COMMAND;
            action.selector = MOTOR_COMMAND_STARTUP_SECOND_INFO_SELECTOR;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO:
        if (input.response_ready && input.command != UINT8_MAX) {
            startup->phase = MOTOR_COMMAND_STARTUP_CONFIRM;
        }
        break;
    case MOTOR_COMMAND_STARTUP_CONFIRM:
        if (input.response_ready) {
            startup->phase = MOTOR_COMMAND_STARTUP_FINISH;
        }
        break;
    case MOTOR_COMMAND_STARTUP_FINISH:
        command_transport_release(transport, MOTOR_COMMAND_STARTUP_OWNER);
        startup->active = false;
        startup->complete = true;
        startup->phase = MOTOR_COMMAND_STARTUP_DONE;
        break;
    case MOTOR_COMMAND_STARTUP_DONE:
        break;
    }
    return action;
}
