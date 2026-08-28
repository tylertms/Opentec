#include "motor/command_scheduler.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    MOTOR_COMMAND_SCHEDULER_RETRY_LIMIT = 2,
};

/**
 * @brief Initializes motor-command service scheduling.
 *
 * Starts the transmit watchdog at one service interval and clears its retry count.
 *
 * @param[out] scheduler Scheduler state to initialize.
 */
void motor_command_scheduler_init(MotorCommandScheduler *scheduler) {
    scheduler->timeout_ticks = MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS;
    scheduler->retry_count = 0;
}

/**
 * @brief Advances motor-command service scheduling by one tick.
 *
 * Resets the watchdog while no transmission awaits acknowledgement. A pending transmission is
 * retried twice at 100-tick intervals before a sequence-reset command is selected. Status writes
 * take priority over command writes, and command writes defer the watchdog by another interval.
 *
 * @param[in,out] scheduler Scheduler state to advance.
 * @param[in] input Current transmission, write, command, and link state.
 * @return Selected service, optional retry command, and protocol-reset request.
 */
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
