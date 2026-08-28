#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/command_scheduler.h"

static MotorCommandSchedulerDecision run(MotorCommandScheduler *scheduler, bool transmit_pending,
                                         bool status_pending, bool command_pending, bool link_ready,
                                         uint8_t command) {
    MotorCommandSchedulerInput input = {
        .transmit_pending = transmit_pending,
        .status_write_pending = status_pending,
        .command_write_pending = command_pending,
        .link_ready = link_ready,
        .pending_command = command,
    };
    return motor_command_scheduler_run(scheduler, &input);
}

static void test_resets_idle_watchdog(void) {
    MotorCommandScheduler scheduler = {0, 9};

    MotorCommandSchedulerDecision decision = run(&scheduler, false, false, false, true, 0x42);

    assert(scheduler.timeout_ticks == MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS);
    assert(scheduler.retry_count == 0);
    assert(decision.service == MOTOR_COMMAND_SERVICE_PROTOCOL);
    assert(!decision.command_ready);
    assert(!decision.reset_protocol);
}

static void test_retries_then_resets_sequence(void) {
    MotorCommandScheduler scheduler;
    motor_command_scheduler_init(&scheduler);

    scheduler.timeout_ticks = 1;
    assert(!run(&scheduler, true, false, false, true, 0x42).command_ready);
    assert(scheduler.timeout_ticks == 0);

    MotorCommandSchedulerDecision decision = run(&scheduler, true, false, false, true, 0x42);
    assert(decision.command_ready);
    assert(decision.command == 0x42);
    assert(scheduler.retry_count == 1);

    scheduler.timeout_ticks = 0;
    decision = run(&scheduler, true, false, false, true, 0x42);
    assert(decision.command == 0x42);
    assert(scheduler.retry_count == 2);

    scheduler.timeout_ticks = 0;
    decision = run(&scheduler, true, false, false, true, 0x42);
    assert(decision.command == MOTOR_COMMAND_SCHEDULER_SEQUENCE_RESET);
    assert(scheduler.retry_count == 1);
}

static void test_prioritizes_writes_and_defers_watchdog(void) {
    MotorCommandScheduler scheduler;
    motor_command_scheduler_init(&scheduler);

    MotorCommandSchedulerDecision decision = run(&scheduler, true, true, true, true, 0);
    assert(decision.service == MOTOR_COMMAND_SERVICE_STATUS_WRITE);
    assert(scheduler.timeout_ticks == MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS - 1);

    decision = run(&scheduler, true, false, true, true, 0);
    assert(decision.service == MOTOR_COMMAND_SERVICE_COMMAND_WRITE);
    assert(scheduler.timeout_ticks == MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS * 2 - 2);
}

static void test_requests_reset_until_link_is_ready(void) {
    MotorCommandScheduler scheduler;
    motor_command_scheduler_init(&scheduler);

    assert(run(&scheduler, false, false, false, false, 0).reset_protocol);
    assert(!run(&scheduler, false, false, false, true, 0).reset_protocol);
}

int main(void) {
    test_resets_idle_watchdog();
    test_retries_then_resets_sequence();
    test_prioritizes_writes_and_defers_watchdog();
    test_requests_reset_until_link_is_ready();
    return 0;
}
