#ifndef OPENTEC_BASE_MOTOR_COMMAND_SCHEDULER_H
#define OPENTEC_BASE_MOTOR_COMMAND_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

enum {
    MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS = 100,
    MOTOR_COMMAND_SCHEDULER_SEQUENCE_RESET = 0xfe,
};

typedef enum {
    MOTOR_COMMAND_SERVICE_PROTOCOL,
    MOTOR_COMMAND_SERVICE_STATUS_WRITE,
    MOTOR_COMMAND_SERVICE_COMMAND_WRITE,
} MotorCommandService;

typedef struct {
    bool transmit_pending;
    bool status_write_pending;
    bool command_write_pending;
    bool link_ready;
    uint8_t pending_command;
} MotorCommandSchedulerInput;

typedef struct {
    MotorCommandService service;
    bool reset_protocol;
    bool command_ready;
    uint8_t command;
} MotorCommandSchedulerDecision;

typedef struct {
    uint32_t timeout_ticks;
    uint8_t retry_count;
} MotorCommandScheduler;

void motor_command_scheduler_init(MotorCommandScheduler *scheduler);
MotorCommandSchedulerDecision motor_command_scheduler_run(MotorCommandScheduler *scheduler,
                                                          const MotorCommandSchedulerInput *input);

#endif
