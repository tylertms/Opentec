#ifndef OPENTEC_BASE_MOTOR_STATUS_SERVICE_H
#define OPENTEC_BASE_MOTOR_STATUS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/output_interlock.h"

/**
 * @brief Phases of the motor command and status exchange.
 *
 * Selects the protocol-specific handshake, status initialization, and periodic status read.
 */
typedef enum {
    MOTOR_STATUS_DISABLED,      /**< Status exchange is unsupported by the identified controller. */
    MOTOR_STATUS_READ_COMMAND,  /**< Reading the extended controller command response. */
    MOTOR_STATUS_WRITE_COMMAND, /**< Writing the position-sensor test command. */
    MOTOR_STATUS_INITIALIZE,    /**< Initializing the controller status register. */
    MOTOR_STATUS_READ,          /**< Reading the controller status register. */
} MotorStatusPhase;

/**
 * @brief Position-sensor test events produced by the motor status exchange.
 *
 * Events are retained until motor_status_service_take_event() consumes them.
 */
typedef enum {
    MOTOR_STATUS_EVENT_NONE = 0, /**< No position-sensor event is pending. */
    MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_SUCCEEDED =
        3,                                               /**< The position-sensor test succeeded. */
    MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_STARTED = 4, /**< The position-sensor test started. */
    MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_FAILED = 5,  /**< The position-sensor test failed. */
} MotorStatusEvent;

/**
 * @brief Motor command/status service state.
 *
 * Stores protocol identity, handshake progress, status responses, pending events, and auxiliary-bus
 * transfer ownership for the periodic exchange.
 */
typedef struct {
    MotorOutputInterlock interlock; /**< Latched output inhibit from qualifying responses. */
    const MotorIdentity *identity;  /**< Identified motor-controller protocol. */
    MotorStatusPhase phase;         /**< Current command or status exchange phase. */
    uint32_t next_cycle_ms;         /**< Monotonic deadline for the next periodic status cycle. */
    uint8_t command[2];             /**< Little-endian command-register response or request word. */
    uint8_t status;                 /**< Current one-byte status-register response. */
    MotorStatusEvent event;         /**< Pending position-sensor event. */
    bool command_pending;           /**< True when a position-sensor test should be requested. */
    bool command_sent;              /**< True after the position-sensor test request was written. */
    bool status_initialized;        /**< True after the initial status-register write completed. */
    bool transfer_active; /**< True while an auxiliary-bus command/status transfer is active. */
} MotorStatusService;

/**
 * @brief Initializes the motor command/status service.
 *
 * Selects the protocol-specific starting phase and clears pending commands, events, transfers, and
 * the output interlock.
 *
 * @param[out] service Motor command/status service state to initialize.
 * @param[in] identity Identified motor-controller protocol.
 */
void motor_status_service_init(MotorStatusService *service, const MotorIdentity *identity);

/**
 * @brief Requests a position-sensor test command.
 *
 * Marks the extended command handshake so the test request is sent during the next eligible cycle.
 *
 * @param[in,out] service Motor command/status service state.
 */
void motor_status_service_request_command(MotorStatusService *service);

/**
 * @brief Advances the motor command/status exchange.
 *
 * Completes active auxiliary-bus transfers, updates the output interlock and pending events, and
 * starts the next eligible command or periodic status transfer.
 *
 * @param[in,out] service Motor command/status service state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_status_service_run(MotorStatusService *service, uint32_t now_ms);

/**
 * @brief Takes the pending position-sensor test event.
 *
 * Returns the retained event and clears the service event slot so it is reported only once.
 *
 * @param[in,out] service Motor command/status service state.
 * @return Pending position-sensor event, or MOTOR_STATUS_EVENT_NONE when none is pending.
 */
MotorStatusEvent motor_status_service_take_event(MotorStatusService *service);

/**
 * @brief Reports whether motor output is inhibited.
 *
 * Reads the latched inhibit state set by a qualifying motor command or status response.
 *
 * @param[in] service Motor command/status service state.
 * @return True when motor output is inhibited.
 */
bool motor_status_service_output_inhibited(const MotorStatusService *service);

#endif
