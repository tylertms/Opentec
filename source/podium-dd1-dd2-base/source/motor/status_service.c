#include "motor/status_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/output_interlock.h"
#include "platform/aux_bus.h"
#include "platform/time.h"

enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78,
    MOTOR_STATUS_REGISTER = 0x04,
    MOTOR_STATUS_POLL_INTERVAL_MS = 200,
};

static bool status_exchange_supported(const MotorIdentity *identity) {
    return identity->protocol != MOTOR_PROTOCOL_LEGACY;
}

/**
 * @brief Initializes periodic motor status synchronization and its output interlock.
 * @param[out] service Motor status service state.
 * @param[in] identity Identified motor-controller protocol.
 */
void motor_status_service_init(MotorStatusService *service, const MotorIdentity *identity) {
    motor_output_interlock_init(&service->interlock);
    service->identity = identity;
    service->phase =
        status_exchange_supported(identity) ? MOTOR_STATUS_INITIALIZE : MOTOR_STATUS_DISABLED;
    service->next_read_ms = 0;
    service->status = 0;
    service->transfer_active = false;
}

static void finish_transfer(MotorStatusService *service, bool succeeded, uint32_t now_ms) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }

    if (service->phase == MOTOR_STATUS_INITIALIZE) {
        service->phase = MOTOR_STATUS_READ;
    } else {
        motor_output_interlock_accept_status(&service->interlock, service->identity,
                                             service->status);
        service->next_read_ms = now_ms + MOTOR_STATUS_POLL_INTERVAL_MS;
    }
}

static void start_transfer(MotorStatusService *service) {
    if (service->phase == MOTOR_STATUS_INITIALIZE) {
        service->transfer_active = platform_aux_bus_start_write(
            MOTOR_AUX_BUS_ADDRESS, MOTOR_STATUS_REGISTER, &service->status, 1);
    } else {
        service->transfer_active = platform_aux_bus_start_read(
            MOTOR_AUX_BUS_ADDRESS, MOTOR_STATUS_REGISTER, &service->status, 1);
    }
}

/**
 * @brief Services initialization and periodic reads of the motor status register.
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
        finish_transfer(service, bus_status == PLATFORM_AUX_BUS_SUCCEEDED, now_ms);
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    if (bus_status == PLATFORM_AUX_BUS_IDLE &&
        (service->phase == MOTOR_STATUS_INITIALIZE ||
         platform_time_reached(now_ms, service->next_read_ms))) {
        start_transfer(service);
    }
}

/**
 * @brief Reports whether a motor status response has latched the output interlock.
 * @param[in] service Motor status service state.
 * @return True after the protocol-specific inhibit response is received.
 */
bool motor_status_service_output_inhibited(const MotorStatusService *service) {
    return motor_output_interlock_engaged(&service->interlock);
}
