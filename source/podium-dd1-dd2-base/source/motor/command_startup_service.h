#ifndef OPENTEC_BASE_MOTOR_COMMAND_STARTUP_SERVICE_H
#define OPENTEC_BASE_MOTOR_COMMAND_STARTUP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "motor/command_startup.h"
#include "transfer/command.h"

/**
 * @brief Result of one motor-command startup service step.
 */
typedef enum {
    MOTOR_COMMAND_STARTUP_SERVICE_RUNNING,  /**< Startup still has asynchronous work pending. */
    MOTOR_COMMAND_STARTUP_SERVICE_COMPLETE, /**< Startup finished and released the transport. */
    MOTOR_COMMAND_STARTUP_SERVICE_FAILED,   /**< Startup stopped after an invalid input or transfer
                                               failure. */
} MotorCommandStartupServiceResult;

/**
 * @brief Pending write category tracked by the startup service.
 */
typedef enum {
    MOTOR_COMMAND_STARTUP_WRITE_NONE,  /**< No startup packet is awaiting mailbox completion. */
    MOTOR_COMMAND_STARTUP_WRITE_RESET, /**< Sequence-reset packet is awaiting mailbox completion. */
    MOTOR_COMMAND_STARTUP_WRITE_REQUEST, /**< Digest or information request is awaiting completion.
                                          */
    MOTOR_COMMAND_STARTUP_WRITE_CONTROL, /**< Acknowledgement, retry control, or retained-payload
                                            retransmission is pending. */
} MotorCommandStartupWrite;

/**
 * @brief Persistent asynchronous motor-command startup service state.
 */
typedef struct {
    MotorCommandStartup startup;                              /**< Startup action planner state. */
    uint8_t length_record[MOTOR_COMMAND_MAILBOX_LENGTH_SIZE]; /**< Mailbox length-read response
                                                                 storage. */
    uint8_t current_command; /**< Command opcode currently supplied to the startup planner. */
    MotorCommandStartupWrite pending_write; /**< Mailbox write currently awaiting completion. */
    bool status_read_pending; /**< True while the initial mailbox-length read is pending. */
    bool response_ready;      /**< True when a startup response is ready for planner consumption. */
    bool failed;              /**< Latched true after startup failure. */
} MotorCommandStartupService;

/**
 * @brief Initializes the asynchronous motor-command startup service.
 *
 * Clears service progress and initializes its startup planner at the transport-release phase. The
 * service pointer must be non-null.
 *
 * @param[out] service Startup service state to initialize.
 */
void motor_command_startup_service_init(MotorCommandStartupService *service);

/**
 * @brief Advances motor-command startup through the mailbox exchange.
 *
 * Coordinates transport ownership, mailbox length discovery, startup request writes, protocol
 * acknowledgements, and application responses. The initial length-read result only gates
 * progression; rejected reads advance to reset like successful reads. Later transfer failures
 * release the startup owner and latch a failed result. Completed startup remains complete on later
 * calls. A null pointer or a previously failed service returns
 * MOTOR_COMMAND_STARTUP_SERVICE_FAILED.
 *
 * @param[in,out] service Startup service state to update.
 * @param[in,out] channel Motor-command channel used to encode and decode packets.
 * @param[in,out] exchange Mailbox exchange used to move packets to and from the motor.
 * @param[in,out] transport Shared command transport used by the mailbox.
 * @return Running, complete, or failed startup status.
 */
MotorCommandStartupServiceResult
motor_command_startup_service_run(MotorCommandStartupService *service, MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport);

#endif
