#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/command_startup.h"

static MotorCommandStartupAction run(MotorCommandStartup *startup, uint8_t command,
                                     bool length_pending, bool response_ready, bool restart) {
    MotorCommandStartupInput input = {command, length_pending, response_ready, restart};
    return motor_command_startup_run(startup, &input);
}

static void test_sequences_startup_actions(void) {
    MotorCommandStartup startup;
    motor_command_startup_init(&startup);

    assert(run(&startup, 0, false, false, false).type == MOTOR_COMMAND_STARTUP_ACTION_RELEASE);
    assert(run(&startup, 0, false, false, false).type == MOTOR_COMMAND_STARTUP_ACTION_CLAIM);
    assert(startup.active);
    assert(run(&startup, 0, false, false, false).type == MOTOR_COMMAND_STARTUP_ACTION_READ_LENGTH);
    assert(run(&startup, 0, true, false, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);

    MotorCommandStartupAction action = run(&startup, 0, false, false, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND);
    assert(run(&startup, MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND, false, false, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);

    action = run(&startup, 0, false, false, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_DIGEST_COMMAND);
    assert(run(&startup, 0, false, false, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);

    action = run(&startup, 0, false, true, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_INFO_COMMAND);
    assert(action.selector == MOTOR_COMMAND_STARTUP_FIRST_INFO_SELECTOR);

    action = run(&startup, 0, false, true, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_INFO_COMMAND);
    assert(action.selector == MOTOR_COMMAND_STARTUP_SECOND_INFO_SELECTOR);
    assert(run(&startup, UINT8_MAX, false, true, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(startup.phase == MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO);

    assert(run(&startup, 0, false, true, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(startup.phase == MOTOR_COMMAND_STARTUP_CONFIRM);
    assert(run(&startup, 0, false, false, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(run(&startup, 0, false, true, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);

    assert(run(&startup, 0, false, true, false).type == MOTOR_COMMAND_STARTUP_ACTION_RELEASE);
    assert(startup.complete);
    assert(!startup.active);
    assert(run(&startup, 0, false, true, false).type == MOTOR_COMMAND_STARTUP_ACTION_NONE);
}

static void test_restart_returns_to_reset(void) {
    MotorCommandStartup startup;
    motor_command_startup_init(&startup);
    (void)run(&startup, 0, false, false, false);
    (void)run(&startup, 0, false, false, false);

    assert(run(&startup, 0, false, false, true).type == MOTOR_COMMAND_STARTUP_ACTION_RELEASE);
    assert(!startup.active);
    assert(!startup.complete);
    assert(startup.phase == MOTOR_COMMAND_STARTUP_CLAIM);
}

int main(void) {
    test_sequences_startup_actions();
    test_restart_returns_to_reset();
    return 0;
}
