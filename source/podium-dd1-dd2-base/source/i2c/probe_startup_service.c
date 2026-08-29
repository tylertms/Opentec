#include "i2c/probe_startup_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"
#include "i2c/probe_bus.h"
#include "platform/aux_bus.h"

/**
 * @brief Initializes the accessory probe startup service.
 *
 * Clears transaction ownership and leaves startup dormant until explicitly requested.
 *
 * @param[out] service Startup service state.
 */
void i2c_probe_startup_service_init(I2cProbeStartupService *service) {
    *service = (I2cProbeStartupService){0};
}

/**
 * @brief Starts a dormant accessory probe startup sequence.
 *
 * Initializes the protocol sequence when the service is idle or complete. A sequence already in
 * progress continues without interruption.
 *
 * @param[in,out] service Startup service state.
 */
void i2c_probe_startup_service_start(I2cProbeStartupService *service) {
    if (service->status == I2C_PROBE_STARTUP_SERVICE_RUNNING) {
        return;
    }

    i2c_probe_startup_init(&service->startup);
    service->status = I2C_PROBE_STARTUP_SERVICE_RUNNING;
    service->transfer_active = false;
}

/**
 * @brief Applies one completed accessory startup transaction.
 *
 * Releases the shared bus, retries failed transactions at the current command, and converts a
 * successful response into the length, status, and payload fields consumed by the startup
 * protocol sequence.
 *
 * @param[in,out] service Startup service state and response buffer.
 * @param[in] succeeded True when the auxiliary-bus transaction succeeded.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void complete_transfer(I2cProbeStartupService *service, bool succeeded, uint32_t now_ms) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }

    const I2cProbeRequest *request = i2c_probe_request_lookup(service->active_command);
    service->response_view = (I2cProbeStartupResponse){
        .declared_length = request->response_length == 0 ? 0 : service->response[0],
        .status = request->response_length < 2 ? 0 : service->response[1],
        .payload = request->response_length <= 2 ? 0 : &service->response[2],
        .payload_length = request->response_length <= 2 ? 0 : request->response_length - 2,
    };
    i2c_probe_startup_accept(&service->startup, service->active_command,
                             request->response_length == 0 ? 0 : &service->response_view, now_ms);
    if (service->startup.complete) {
        service->status = I2C_PROBE_STARTUP_SERVICE_COMPLETE;
    }
}

/**
 * @brief Advances the accessory probe startup service.
 *
 * Waits for an owned transaction, applies its terminal result, and starts the next protocol
 * command only while the shared auxiliary bus is idle. A failed start or transaction remains
 * available for retry on a later call.
 *
 * @param[in,out] service Startup service state and response buffer.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void i2c_probe_startup_service_run(I2cProbeStartupService *service, uint32_t now_ms) {
    if (service->status != I2C_PROBE_STARTUP_SERVICE_RUNNING) {
        return;
    }

    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        complete_transfer(service, bus_status == PLATFORM_AUX_BUS_SUCCEEDED, now_ms);
        return;
    }
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return;
    }

    if (!i2c_probe_startup_current(&service->startup, now_ms, &service->active_command)) {
        return;
    }

    const I2cProbeRequest *request = i2c_probe_request_lookup(service->active_command);
    service->transfer_active = i2c_probe_bus_start(
        service->active_command, request->response_length == 0 ? 0 : service->response);
}

/**
 * @brief Returns the current accessory probe startup service status.
 *
 * Reports whether startup is dormant, in progress, or complete without exposing transaction
 * ownership details.
 *
 * @param[in] service Startup service state.
 * @return Current startup service status.
 */
I2cProbeStartupServiceStatus
i2c_probe_startup_service_status(const I2cProbeStartupService *service) {
    return service->status;
}
