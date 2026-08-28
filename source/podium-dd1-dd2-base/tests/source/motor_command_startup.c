#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/command_startup.h"

static MotorCommandStartupAction run(MotorCommandStartup *startup, CommandTransport *transport,
                                     uint8_t command, bool status_pending, bool response_ready,
                                     bool restart) {
    MotorCommandStartupInput input = {command, status_pending, response_ready, restart};
    return motor_command_startup_run(startup, transport, &input);
}

static void test_sequences_startup_actions(void) {
    MotorCommandStartup startup;
    CommandTransport transport;
    motor_command_startup_init(&startup);
    command_transport_init(&transport);

    assert(run(&startup, &transport, 0, false, false, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(command_transport_is_owner(&transport, 0));
    assert(run(&startup, &transport, 0, false, false, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(command_transport_is_owner(&transport, MOTOR_COMMAND_STARTUP_OWNER));
    assert(startup.active);
    MotorCommandStartupAction action = run(&startup, &transport, 0, false, false, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_READ_STATUS);
    assert(run(&startup, &transport, 0, true, false, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);

    action = run(&startup, &transport, 0, false, false, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND);
    assert(
        run(&startup, &transport, MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND, false, false, false)
            .type == MOTOR_COMMAND_STARTUP_ACTION_NONE);

    action = run(&startup, &transport, 0, false, false, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_DIGEST_COMMAND);
    assert(run(&startup, &transport, 0, false, false, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);

    action = run(&startup, &transport, 0, false, true, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_INFO_COMMAND);
    assert(action.selector == MOTOR_COMMAND_STARTUP_FIRST_INFO_SELECTOR);

    action = run(&startup, &transport, 0, false, true, false);
    assert(action.type == MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND);
    assert(action.command == MOTOR_COMMAND_STARTUP_INFO_COMMAND);
    assert(action.selector == MOTOR_COMMAND_STARTUP_SECOND_INFO_SELECTOR);
    assert(run(&startup, &transport, UINT8_MAX, false, true, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(startup.phase == MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO);

    assert(run(&startup, &transport, 0, false, true, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(startup.phase == MOTOR_COMMAND_STARTUP_CONFIRM);
    assert(run(&startup, &transport, 0, false, false, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(run(&startup, &transport, 0, false, true, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);

    assert(run(&startup, &transport, 0, false, true, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(startup.complete);
    assert(!startup.active);
    assert(command_transport_is_owner(&transport, 0));
    assert(run(&startup, &transport, 0, false, true, false).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
}

static void test_restart_returns_to_reset(void) {
    MotorCommandStartup startup;
    CommandTransport transport;
    motor_command_startup_init(&startup);
    command_transport_init(&transport);
    (void)run(&startup, &transport, 0, false, false, false);
    (void)run(&startup, &transport, 0, false, false, false);
    assert(command_transport_is_owner(&transport, MOTOR_COMMAND_STARTUP_OWNER));

    assert(run(&startup, &transport, 0, false, false, true).type ==
           MOTOR_COMMAND_STARTUP_ACTION_NONE);
    assert(!startup.active);
    assert(!startup.complete);
    assert(startup.phase == MOTOR_COMMAND_STARTUP_CLAIM);
    assert(command_transport_is_owner(&transport, 0));
}

int main(void) {
    test_sequences_startup_actions();
    test_restart_returns_to_reset();
    return 0;
}
