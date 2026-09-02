#include "motor/command_scheduler.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Internal retry policy for pending motor-command transmissions.
 */
enum {
    MOTOR_COMMAND_SCHEDULER_RETRY_LIMIT = 2, /**< Number of retries before recovery command. */
};

void motor_command_scheduler_init(MotorCommandScheduler *scheduler) {
    scheduler->timeout_ticks = MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS;
    scheduler->retry_count = 0;
}

MotorCommandSchedulerDecision motor_command_scheduler_run(MotorCommandScheduler *scheduler,
                                                          const MotorCommandSchedulerInput *input) {
    MotorCommandSchedulerDecision decision = {
        .service = MOTOR_COMMAND_SERVICE_PROTOCOL,
        .reset_protocol = !input->link_ready,
    };
    if (!input->transmit_pending) {
        motor_command_scheduler_init(scheduler);
    } else if (scheduler->timeout_ticks != 0) {
        scheduler->timeout_ticks--;
    } else {
        decision.command_ready = true;
        if (scheduler->retry_count < MOTOR_COMMAND_SCHEDULER_RETRY_LIMIT) {
            decision.command = input->pending_command;
        } else {
            decision.command = MOTOR_COMMAND_SCHEDULER_SEQUENCE_RESET;
            scheduler->retry_count = 0;
        }
        scheduler->retry_count++;
        scheduler->timeout_ticks = MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS;
    }
    if (input->status_write_pending) {
        decision.service = MOTOR_COMMAND_SERVICE_STATUS_WRITE;
    } else if (input->command_write_pending) {
        decision.service = MOTOR_COMMAND_SERVICE_COMMAND_WRITE;
        scheduler->timeout_ticks += MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS;
    }
    return decision;
}
