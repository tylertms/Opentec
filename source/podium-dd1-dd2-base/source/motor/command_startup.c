#include "motor/command_startup.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the motor-command startup sequence.
 *
 * Clears the active and completion flags and selects the reset phase that releases transport owner
 * 0x20 before the next startup attempt.
 *
 * @param[out] startup Startup state to initialize.
 */
void motor_command_startup_init(MotorCommandStartup *startup) {
    startup->phase = MOTOR_COMMAND_STARTUP_RESET;
    startup->active = false;
    startup->complete = false;
}

/**
 * @brief Advances the motor-command mailbox startup sequence.
 *
 * Claims transport owner 0x20, requests the remote length, then sequences reset, digest, and
 * command-5 information selectors 3 and 4. Completion releases the transport after both
 * information responses are observed. A restart input returns the sequence to its reset phase
 * immediately.
 *
 * @param[in,out] startup Startup state to advance.
 * @param[in] input Current command, response, length-read, and restart state.
 * @return Next transport or command action, or no action while an expected event is pending.
 */
MotorCommandStartupAction motor_command_startup_run(MotorCommandStartup *startup,
                                                    const MotorCommandStartupInput *input) {
    MotorCommandStartupAction action = {MOTOR_COMMAND_STARTUP_ACTION_NONE, 0, 0};
    if (input->restart) {
        motor_command_startup_init(startup);
    }

    switch (startup->phase) {
    case MOTOR_COMMAND_STARTUP_RESET:
        startup->phase = MOTOR_COMMAND_STARTUP_CLAIM;
        action.type = MOTOR_COMMAND_STARTUP_ACTION_RELEASE;
        break;
    case MOTOR_COMMAND_STARTUP_CLAIM:
        startup->active = true;
        startup->phase = MOTOR_COMMAND_STARTUP_READ_LENGTH;
        action.type = MOTOR_COMMAND_STARTUP_ACTION_CLAIM;
        break;
    case MOTOR_COMMAND_STARTUP_READ_LENGTH:
        startup->phase = MOTOR_COMMAND_STARTUP_WAIT_LENGTH;
        action.type = MOTOR_COMMAND_STARTUP_ACTION_READ_LENGTH;
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_LENGTH:
        if (!input->length_read_pending) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_RESET;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_RESET:
        if (input->command == 0) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_DIGEST;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_DIGEST_COMMAND;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_DIGEST:
        if (input->response_ready) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_INFO_COMMAND;
            action.selector = MOTOR_COMMAND_STARTUP_FIRST_INFO_SELECTOR;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO:
        if (input->response_ready) {
            startup->phase = MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO;
            action.type = MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND;
            action.command = MOTOR_COMMAND_STARTUP_INFO_COMMAND;
            action.selector = MOTOR_COMMAND_STARTUP_SECOND_INFO_SELECTOR;
        }
        break;
    case MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO:
        if (input->response_ready && input->command != UINT8_MAX) {
            startup->phase = MOTOR_COMMAND_STARTUP_CONFIRM;
        }
        break;
    case MOTOR_COMMAND_STARTUP_CONFIRM:
        if (input->response_ready) {
            startup->phase = MOTOR_COMMAND_STARTUP_FINISH;
        }
        break;
    case MOTOR_COMMAND_STARTUP_FINISH:
        startup->active = false;
        startup->complete = true;
        startup->phase = MOTOR_COMMAND_STARTUP_DONE;
        action.type = MOTOR_COMMAND_STARTUP_ACTION_RELEASE;
        break;
    case MOTOR_COMMAND_STARTUP_DONE:
        break;
    }
    return action;
}
