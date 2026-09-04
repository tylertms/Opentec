#ifndef OPENTEC_BASE_MOTOR_COMMAND_STARTUP_H
#define OPENTEC_BASE_MOTOR_COMMAND_STARTUP_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/**
 * @brief Motor-command startup protocol constants.
 */
enum {
    MOTOR_COMMAND_STARTUP_OWNER = 0x20, /**< Shared command-transport owner identifier. */
    MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND =
        0xfe,                                 /**< Command that resets peer sequencing. */
    MOTOR_COMMAND_STARTUP_DIGEST_COMMAND = 7, /**< Command that reads the calibration digest. */
    MOTOR_COMMAND_STARTUP_INFO_COMMAND = 5, /**< Command that reads a motor information selector. */
    MOTOR_COMMAND_STARTUP_FIRST_INFO_SELECTOR =
        3, /**< First information selector read at startup. */
    MOTOR_COMMAND_STARTUP_SECOND_INFO_SELECTOR =
        4, /**< Second information selector read at startup. */
};

/**
 * @brief Phase of the motor-command startup sequence.
 */
typedef enum {
    MOTOR_COMMAND_STARTUP_RESET,            /**< Release any stale startup transport ownership. */
    MOTOR_COMMAND_STARTUP_CLAIM,            /**< Attempt to claim the shared command transport. */
    MOTOR_COMMAND_STARTUP_READ_STATUS,      /**< Schedule the initial mailbox-length read. */
    MOTOR_COMMAND_STARTUP_WAIT_STATUS,      /**< Wait for the initial mailbox-length read. */
    MOTOR_COMMAND_STARTUP_WAIT_RESET,       /**< Wait for the sequence-reset write to complete. */
    MOTOR_COMMAND_STARTUP_WAIT_DIGEST,      /**< Wait for the calibration digest response. */
    MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO,  /**< Wait for information selector three. */
    MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO, /**< Wait for information selector four. */
    MOTOR_COMMAND_STARTUP_CONFIRM, /**< Confirm the second information response before finish. */
    MOTOR_COMMAND_STARTUP_FINISH,  /**< Release transport ownership and mark startup complete. */
    MOTOR_COMMAND_STARTUP_DONE,    /**< Startup is complete and remains idle. */
} MotorCommandStartupPhase;

/**
 * @brief Action requested by the motor-command startup planner.
 */
typedef enum {
    MOTOR_COMMAND_STARTUP_ACTION_NONE,         /**< No transport action is required. */
    MOTOR_COMMAND_STARTUP_ACTION_READ_STATUS,  /**< Read the remote mailbox length. */
    MOTOR_COMMAND_STARTUP_ACTION_SEND_COMMAND, /**< Send the command and optional selector fields.
                                                */
} MotorCommandStartupActionType;

/**
 * @brief Command action selected by the startup planner.
 */
typedef struct {
    MotorCommandStartupActionType type; /**< Requested startup action. */
    uint8_t command;                    /**< Command opcode for a send-command action. */
    uint8_t selector;                   /**< Information selector for command five. */
} MotorCommandStartupAction;

/**
 * @brief Events supplied to the motor-command startup planner.
 */
typedef struct {
    uint8_t command;          /**< Most recently received command opcode. */
    bool status_read_pending; /**< True while the initial mailbox-length read is pending. */
    bool response_ready;      /**< True when the expected startup response is available. */
    bool restart;             /**< True to reinitialize the sequence before advancing it. */
} MotorCommandStartupInput;

/**
 * @brief Persistent state of the motor-command startup planner.
 */
typedef struct {
    MotorCommandStartupPhase phase; /**< Current startup phase. */
    bool active;   /**< True after the planner owns the shared transport. */
    bool complete; /**< True after the release phase completes and the planner enters DONE. */
} MotorCommandStartup;

/**
 * @brief Initializes the motor-command startup planner.
 *
 * Clears active and completion state and starts at the phase that releases stale startup
 * ownership before a new attempt. The startup pointer must be non-null.
 *
 * @param[out] startup Startup planner state to initialize.
 */
void motor_command_startup_init(MotorCommandStartup *startup);

/**
 * @brief Advances the motor-command startup planner.
 *
 * Waits until it owns the shared transport, schedules the initial mailbox read, and sequences the
 * reset, digest, and information-selector requests. A restart reinitializes the planner before that
 * call advances it. The startup and transport pointers must be non-null.
 *
 * @param[in,out] startup Startup planner state to update.
 * @param[in,out] transport Shared command transport used for ownership changes.
 * @param[in] input Current command, response, status-read, and restart events.
 * @return Action to perform next, or no action while the expected event is pending.
 */
MotorCommandStartupAction motor_command_startup_run(MotorCommandStartup *startup,
                                                    CommandTransport *transport,
                                                    MotorCommandStartupInput input);

#endif
