#include "i2c/probe_exchange_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe.h"
#include "i2c/probe_bus.h"
#include "platform/aux_bus.h"

/**
 * @brief Initializes an accessory command exchange service.
 *
 * Clears command, response, protocol, and auxiliary-bus ownership state.
 *
 * @param[out] service Exchange service state.
 */
void i2c_probe_exchange_service_init(I2cProbeExchangeService *service) {
    *service = (I2cProbeExchangeService){0};
}

/**
 * @brief Starts one encoded accessory command exchange.
 *
 * Copies the command frame and begins with the device-ready response poll. An exchange already in
 * progress or awaiting validation is left unchanged.
 *
 * @param[in,out] service Exchange service state.
 * @param[in] frame Encoded write command and expected response layout.
 * @return True when a new exchange starts; otherwise false.
 */
bool i2c_probe_exchange_service_start(I2cProbeExchangeService *service,
                                      const I2cProbeTransferFrame *frame) {
    if (service == 0 || frame == 0 || frame->write_length == 0 || frame->response_length == 0 ||
        frame->response_length > sizeof(service->response) ||
        service->status == I2C_PROBE_EXCHANGE_SERVICE_RUNNING ||
        service->status == I2C_PROBE_EXCHANGE_SERVICE_RESPONSE_READY) {
        return false;
    }

    service->frame = *frame;
    memset(service->status_response, 0, sizeof(service->status_response));
    memset(service->response, 0, sizeof(service->response));
    i2c_probe_exchange_init(&service->exchange);
    service->status = I2C_PROBE_EXCHANGE_SERVICE_RUNNING;
    service->transfer_active = false;
    return true;
}

/**
 * @brief Applies one completed auxiliary-bus operation to the command exchange.
 *
 * Releases the bus result, retries failed operations at the same protocol stage, evaluates ready
 * and acceptance statuses, records a queued command write, or exposes the final response.
 *
 * @param[in,out] service Active exchange service state and response buffers.
 * @param[in] succeeded True when the auxiliary-bus operation succeeded.
 */
static void complete_transfer(I2cProbeExchangeService *service, bool succeeded) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }

    if (service->exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY ||
        service->exchange.stage == I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE) {
        i2c_probe_exchange_status(&service->exchange, service->status_response[1]);
    } else if (service->exchange.stage == I2C_PROBE_EXCHANGE_QUEUE_COMMAND) {
        i2c_probe_exchange_command_queued(&service->exchange);
    } else if (service->exchange.stage == I2C_PROBE_EXCHANGE_WAIT_RESPONSE) {
        service->status = I2C_PROBE_EXCHANGE_SERVICE_RESPONSE_READY;
    }

    if (service->exchange.stage == I2C_PROBE_EXCHANGE_FAILED) {
        service->status = I2C_PROBE_EXCHANGE_SERVICE_FAILED;
    }
}

/**
 * @brief Advances an accessory command exchange.
 *
 * Services one owned transaction or, while the shared bus is idle, starts the ready poll, command
 * write, acceptance poll, or final response read selected by the protocol stage.
 *
 * @param[in,out] service Exchange service state and response buffers.
 */
void i2c_probe_exchange_service_run(I2cProbeExchangeService *service) {
    if (service == 0 || service->status != I2C_PROBE_EXCHANGE_SERVICE_RUNNING) {
        return;
    }

    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        complete_transfer(service, bus_status == PLATFORM_AUX_BUS_SUCCEEDED);
        return;
    }
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return;
    }

    if (service->exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY ||
        service->exchange.stage == I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE) {
        service->transfer_active =
            i2c_probe_bus_start(I2C_PROBE_READ_READY_STATUS, service->status_response);
    } else if (service->exchange.stage == I2C_PROBE_EXCHANGE_QUEUE_COMMAND) {
        service->transfer_active = i2c_probe_bus_start_frame_write(&service->frame);
    } else if (service->exchange.stage == I2C_PROBE_EXCHANGE_WAIT_RESPONSE) {
        service->transfer_active =
            i2c_probe_bus_start_frame_read(&service->frame, service->response);
    }
}

/**
 * @brief Returns a completed raw accessory response.
 *
 * Makes the final register-0x82 response available only while it awaits protocol validation.
 *
 * @param[in] service Exchange service state and response buffer.
 * @param[out] length Response length when a response is ready.
 * @return Response bytes while validation is pending; otherwise null.
 */
const uint8_t *i2c_probe_exchange_service_response(const I2cProbeExchangeService *service,
                                                   uint8_t *length) {
    if (service == 0 || length == 0 ||
        service->status != I2C_PROBE_EXCHANGE_SERVICE_RESPONSE_READY) {
        return 0;
    }
    *length = service->frame.response_length;
    return service->response;
}

/**
 * @brief Completes validation of an accessory response.
 *
 * Applies the response status and optional checksum result, then records either successful
 * completion or the classified protocol failure.
 *
 * @param[in,out] service Exchange awaiting response validation.
 * @param[in] response Parsed response payload, checksum, and status fields.
 * @param[in] checksum_enabled True to compare the payload checksum.
 * @return True when the response belonged to the pending exchange; otherwise false.
 */
bool i2c_probe_exchange_service_finish(I2cProbeExchangeService *service,
                                       const I2cProbeFinalResponse *response,
                                       bool checksum_enabled) {
    if (service == 0 || response == 0 ||
        service->status != I2C_PROBE_EXCHANGE_SERVICE_RESPONSE_READY ||
        !i2c_probe_exchange_finalize(&service->exchange, response, checksum_enabled)) {
        return false;
    }

    service->status = service->exchange.stage == I2C_PROBE_EXCHANGE_COMPLETE
                          ? I2C_PROBE_EXCHANGE_SERVICE_COMPLETE
                          : I2C_PROBE_EXCHANGE_SERVICE_FAILED;
    return true;
}

/**
 * @brief Returns the accessory command exchange service status.
 *
 * Reports whether the service is dormant, transferring, awaiting validation, complete, or failed.
 *
 * @param[in] service Exchange service state.
 * @return Current service status, or idle for a null service.
 */
I2cProbeExchangeServiceStatus
i2c_probe_exchange_service_status(const I2cProbeExchangeService *service) {
    return service == 0 ? I2C_PROBE_EXCHANGE_SERVICE_IDLE : service->status;
}

/**
 * @brief Returns the accessory command protocol result.
 *
 * Exposes the pending, success, command, checksum, or response classification from the protocol
 * exchange.
 *
 * @param[in] service Exchange service state.
 * @return Current protocol result, or pending for a null service.
 */
I2cProbeExchangeResult i2c_probe_exchange_service_result(const I2cProbeExchangeService *service) {
    return service == 0 ? I2C_PROBE_EXCHANGE_PENDING : service->exchange.result;
}
