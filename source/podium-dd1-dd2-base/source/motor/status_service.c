#include "motor/status_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/output_interlock.h"
#include "platform/aux_bus.h"
#include "platform/time.h"

/**
 * @brief Auxiliary-bus registers, timing, and command responses for motor status exchange.
 */
enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78,  /**< Auxiliary-bus address of the motor controller. */
    MOTOR_COMMAND_REGISTER = 0x05, /**< Register for the extended command handshake. */
    MOTOR_STATUS_REGISTER = 0x04,  /**< Register for the periodic motor status byte. */
    MOTOR_STATUS_CYCLE_INTERVAL_MS =
        200,                         /**< Delay between successful status cycles in milliseconds. */
    MOTOR_COMMAND_IDLE = 0x0000,     /**< Response indicating the command register is idle. */
    MOTOR_COMMAND_ACCEPTED = 0xaaaa, /**< Response accepting the command request. */
    MOTOR_COMMAND_FAULT = 0xbbbb,    /**< Response indicating a command fault. */
    MOTOR_COMMAND_UNAVAILABLE = 0xffff, /**< Response indicating the command is unavailable. */
};

/**
 * @brief Tests whether the identified controller supports the status exchange.
 *
 * Legacy controllers omit the command and status registers used by this service.
 *
 * @param[in] identity Identified motor-controller protocol.
 * @return True when command and status exchange is supported.
 */
static bool status_exchange_supported(const MotorIdentity *identity) {
    return identity->protocol != MOTOR_PROTOCOL_LEGACY;
}

/**
 * @brief Initializes the motor-command handshake, status exchange, and output interlock.
 *
 * Selects the protocol-specific starting phase and clears all pending command, transfer, event,
 * and interlock state.
 *
 * @param[out] service Motor status service state.
 * @param[in] identity Identified motor-controller protocol.
 */
void motor_status_service_init(MotorStatusService *service, const MotorIdentity *identity) {
    motor_output_interlock_init(&service->interlock);
    service->identity = identity;
    service->phase = !status_exchange_supported(identity)               ? MOTOR_STATUS_DISABLED
                     : motor_identity_has_extended_parameters(identity) ? MOTOR_STATUS_READ_COMMAND
                                                                        : MOTOR_STATUS_INITIALIZE;
    service->next_cycle_ms = 0;
    service->command[0] = 0;
    service->command[1] = 0;
    service->status = 0;
    service->event = MOTOR_STATUS_EVENT_NONE;
    service->command_pending = false;
    service->command_sent = false;
    service->status_initialized = false;
    service->transfer_active = false;
}

/**
 * @brief Marks the extended motor-command handshake pending.
 *
 * Causes the next idle command-register cycle to submit the fixed 0xABCD request word.
 *
 * @param[in,out] service Motor status service state.
 */
void motor_status_service_request_command(MotorStatusService *service) {
    service->command_pending = true;
}

/**
 * @brief Selects the next status phase after a command exchange.
 *
 * Initializes the status register before the first read and otherwise resumes periodic reads.
 *
 * @param[in,out] service Motor status service state.
 */
static void continue_to_status(MotorStatusService *service) {
    service->phase = service->status_initialized ? MOTOR_STATUS_READ : MOTOR_STATUS_INITIALIZE;
}

/**
 * @brief Decodes the little-endian motor command response.
 *
 * Combines the two command-register bytes into the controller response word.
 *
 * @param[in] service Motor status service state.
 * @return Decoded command response word.
 */
static uint16_t command_response(const MotorStatusService *service) {
    return (uint16_t)service->command[0] | (uint16_t)service->command[1] << 8;
}

/**
 * @brief Applies a completed motor command-register read.
 *
 * Advances the handshake, publishes the corresponding operator notice, and updates the output
 * interlock from terminal controller responses.
 *
 * @param[in,out] service Motor status service state.
 */
static void finish_command_read(MotorStatusService *service) {
    uint16_t response = command_response(service);
    motor_output_interlock_accept_command(&service->interlock, service->identity, response);

    if (response == MOTOR_COMMAND_IDLE) {
        if (service->command_sent) {
            service->event = MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_SUCCEEDED;
            service->command_pending = false;
            service->command_sent = false;
            continue_to_status(service);
        } else if (service->command_pending) {
            service->event = MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_STARTED;
            service->phase = MOTOR_STATUS_WRITE_COMMAND;
        } else {
            continue_to_status(service);
        }
    } else if (response == MOTOR_COMMAND_FAULT) {
        service->event = MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_FAILED;
        service->command_pending = false;
        service->command_sent = false;
        continue_to_status(service);
    } else if (response == MOTOR_COMMAND_ACCEPTED || response == MOTOR_COMMAND_UNAVAILABLE) {
        continue_to_status(service);
    }
}

/**
 * @brief Completes the active auxiliary-bus transfer.
 *
 * Clears the bus transaction, advances successful command and status phases, and leaves failed
 * phases eligible for retry.
 *
 * @param[in,out] service Motor status service state.
 * @param[in] succeeded True when the transfer completed successfully.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void finish_transfer(MotorStatusService *service, bool succeeded, uint32_t now_ms) {
    MotorStatusPhase completed_phase = service->phase;
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }

    if (completed_phase == MOTOR_STATUS_READ_COMMAND) {
        finish_command_read(service);
    } else if (completed_phase == MOTOR_STATUS_WRITE_COMMAND) {
        continue_to_status(service);
    } else if (completed_phase == MOTOR_STATUS_INITIALIZE) {
        service->status_initialized = true;
        service->phase = MOTOR_STATUS_READ;
    } else {
        motor_output_interlock_accept_status(&service->interlock, service->identity,
                                             service->status);
        service->next_cycle_ms = now_ms + MOTOR_STATUS_CYCLE_INTERVAL_MS;
        service->phase = motor_identity_has_extended_parameters(service->identity)
                             ? MOTOR_STATUS_READ_COMMAND
                             : MOTOR_STATUS_READ;
    }
}

/**
 * @brief Starts the transfer required by the current motor status phase.
 *
 * Reads command or status data and writes the fixed command request or initial status byte.
 *
 * @param[in,out] service Motor status service state.
 */
static void start_transfer(MotorStatusService *service) {
    if (service->phase == MOTOR_STATUS_READ_COMMAND) {
        service->transfer_active =
            platform_aux_bus_start_read(MOTOR_AUX_BUS_ADDRESS, MOTOR_COMMAND_REGISTER,
                                        service->command, sizeof(service->command));
    } else if (service->phase == MOTOR_STATUS_WRITE_COMMAND) {
        service->command[0] = 0xcd;
        service->command[1] = 0xab;
        service->command_sent = true;
        service->transfer_active =
            platform_aux_bus_start_write(MOTOR_AUX_BUS_ADDRESS, MOTOR_COMMAND_REGISTER,
                                         service->command, sizeof(service->command));
    } else if (service->phase == MOTOR_STATUS_INITIALIZE) {
        service->transfer_active = platform_aux_bus_start_write(
            MOTOR_AUX_BUS_ADDRESS, MOTOR_STATUS_REGISTER, &service->status, 1);
    } else {
        service->transfer_active = platform_aux_bus_start_read(
            MOTOR_AUX_BUS_ADDRESS, MOTOR_STATUS_REGISTER, &service->status, 1);
    }
}

/**
 * @brief Services the motor-command handshake and periodic motor status exchange.
 *
 * Completes an active auxiliary-bus transfer, advances the command or status phase, and starts the
 * next eligible transfer. Successful status reads repeat every 200 milliseconds.
 *
 * @param[in,out] service Motor status synchronization and interlock state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_status_service_run(MotorStatusService *service, uint32_t now_ms) {
    if (service->phase == MOTOR_STATUS_DISABLED) {
        return;
    }

    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        bool succeeded = bus_status == PLATFORM_AUX_BUS_SUCCEEDED;
        finish_transfer(service, succeeded, now_ms);
        if (!succeeded) {
            return;
        }
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    bool cycle_ready = service->phase == MOTOR_STATUS_WRITE_COMMAND ||
                       service->phase == MOTOR_STATUS_INITIALIZE ||
                       platform_time_reached(now_ms, service->next_cycle_ms);
    if (bus_status == PLATFORM_AUX_BUS_IDLE && cycle_ready) {
        start_transfer(service);
    }
}

/**
 * @brief Takes the pending operator notice produced by the motor-command handshake.
 *
 * Returns the retained event once and clears it so the single-slot system event queue does not
 * repeatedly publish the same notice.
 *
 * @param[in,out] service Motor status service state.
 * @return Pending position-sensor event, or none.
 */
MotorStatusEvent motor_status_service_take_event(MotorStatusService *service) {
    MotorStatusEvent event = service->event;
    service->event = MOTOR_STATUS_EVENT_NONE;
    return event;
}

/**
 * @brief Reports whether a motor command or status response has latched the output interlock.
 *
 * Reads the irreversible inhibit latch owned by the motor status service.
 *
 * @param[in] service Motor status service state.
 * @return True after a protocol-specific inhibit response is received.
 */
bool motor_status_service_output_inhibited(const MotorStatusService *service) {
    return motor_output_interlock_engaged(&service->interlock);
}
