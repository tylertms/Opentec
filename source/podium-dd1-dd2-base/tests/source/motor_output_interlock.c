#include <assert.h>

#include "motor/identity.h"
#include "motor/output_interlock.h"

static MotorIdentity identity(MotorProtocol protocol) {
    MotorIdentity value = {0};
    value.protocol = protocol;
    return value;
}

static void test_initial_state(void) {
    MotorOutputInterlock interlock;

    motor_output_interlock_init(&interlock);

    assert(!motor_output_interlock_engaged(&interlock));
}

static void test_extended_command_response(void) {
    MotorOutputInterlock interlock;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION_A);
    MotorIdentity standard = identity(MOTOR_PROTOCOL_STANDARD);

    motor_output_interlock_init(&interlock);
    motor_output_interlock_accept_command(&interlock, &extended, 0xaaaa);
    motor_output_interlock_accept_command(&interlock, &extended, 0xabcd);
    motor_output_interlock_accept_command(&interlock, &standard, 0xbbbb);
    assert(!motor_output_interlock_engaged(&interlock));

    motor_output_interlock_accept_command(&interlock, &extended, 0xbbbb);
    assert(motor_output_interlock_engaged(&interlock));

    motor_output_interlock_accept_command(&interlock, &extended, 0);
    assert(motor_output_interlock_engaged(&interlock));
}

static void test_extended_status_response(void) {
    MotorOutputInterlock interlock;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION_B);

    motor_output_interlock_init(&interlock);
    motor_output_interlock_accept_status(&interlock, &extended, 0xff);
    assert(!motor_output_interlock_engaged(&interlock));

    motor_output_interlock_accept_status(&interlock, &extended, 0xaa);
    assert(motor_output_interlock_engaged(&interlock));
}

static void test_standard_status_response(void) {
    MotorOutputInterlock interlock;
    MotorIdentity legacy = identity(MOTOR_PROTOCOL_LEGACY);
    MotorIdentity standard = identity(MOTOR_PROTOCOL_STANDARD);

    motor_output_interlock_init(&interlock);
    motor_output_interlock_accept_status(&interlock, &legacy, 0xaa);
    motor_output_interlock_accept_status(&interlock, &standard, 0xaa);
    assert(!motor_output_interlock_engaged(&interlock));

    motor_output_interlock_accept_status(&interlock, &legacy, 0xff);
    assert(motor_output_interlock_engaged(&interlock));
}

int main(void) {
    test_initial_state();
    test_extended_command_response();
    test_extended_status_response();
    test_standard_status_response();
    return 0;
}
