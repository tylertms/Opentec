#ifndef OPENTEC_BASE_MOTOR_COMMAND_SCHEDULER_H
#define OPENTEC_BASE_MOTOR_COMMAND_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Motor-command scheduler constants.
 */
enum {
    MOTOR_COMMAND_SCHEDULER_INTERVAL_TICKS =
        100, /**< Ticks between transmission timeout actions. */
    MOTOR_COMMAND_SCHEDULER_SEQUENCE_RESET =
        0xfe, /**< Application command used to recover a stalled sequence. */
};

/**
 * @brief Service selected for the next motor-command scheduler step.
 */
typedef enum {
    MOTOR_COMMAND_SERVICE_PROTOCOL,      /**< Run the command protocol service. */
    MOTOR_COMMAND_SERVICE_STATUS_WRITE,  /**< Write the pending motor status. */
    MOTOR_COMMAND_SERVICE_COMMAND_WRITE, /**< Write the pending motor command. */
} MotorCommandService;

/**
 * @brief State observed by the motor-command scheduler.
 */
typedef struct {
    bool transmit_pending;      /**< True while a command awaits acknowledgement. */
    bool status_write_pending;  /**< True while a status write is waiting to run. */
    bool command_write_pending; /**< True while a command write is waiting to run. */
    bool link_ready;            /**< True when the motor link is ready for protocol traffic. */
    uint8_t pending_command;    /**< Command opcode to retry after a timeout. */
} MotorCommandSchedulerInput;

/**
 * @brief Work selected by one motor-command scheduler step.
 */
typedef struct {
    MotorCommandService service; /**< Service selected for this step. */
    bool reset_protocol; /**< True when the link is not ready and protocol state must reset. */
    bool command_ready;  /**< True when command contains a timeout retry or reset command. */
    uint8_t command;     /**< Command opcode to transmit when command_ready is true. */
} MotorCommandSchedulerDecision;

/**
 * @brief Persistent motor-command scheduler timing state.
 */
typedef struct {
    uint32_t timeout_ticks; /**< Ticks remaining before the next timeout action. */
    uint8_t retry_count;    /**< Timeout actions emitted for the pending transmission. */
} MotorCommandScheduler;

/**
 * @brief Initializes motor-command scheduler state.
 *
 * Loads one scheduler interval and clears the timeout-action count. The scheduler pointer must be
 * non-null.
 *
 * @param[out] scheduler Scheduler state to initialize.
 */
void motor_command_scheduler_init(MotorCommandScheduler *scheduler);

/**
 * @brief Advances motor-command scheduling by one tick.
 *
 * Reloads idle state, counts down a pending transmission, and selects the pending command for the
 * first two expirations before selecting the recovery command. Status writes take priority over
 * command writes; a selected command write extends the watchdog by one interval. The scheduler and
 * input pointers must be non-null.
 *
 * @param[in,out] scheduler Scheduler timing state to update.
 * @param[in] input Current transmission, write, command, and link state.
 * @return Service selection, protocol-reset request, and optional timeout command.
 */
MotorCommandSchedulerDecision motor_command_scheduler_run(MotorCommandScheduler *scheduler,
                                                          const MotorCommandSchedulerInput *input);

#endif
