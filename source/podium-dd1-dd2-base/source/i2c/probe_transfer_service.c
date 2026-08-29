#include "i2c/probe_transfer_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe.h"
#include "i2c/probe_exchange_service.h"

/**
 * @brief Initializes an accessory data-transfer service.
 *
 * Clears the owned request and response buffers, command sequence, exchange state, and result.
 *
 * @param[out] service Transfer service state.
 */
void i2c_probe_transfer_service_init(I2cProbeTransferService *service) {
    *service = (I2cProbeTransferService){0};
}

/**
 * @brief Starts an accessory data transfer.
 *
 * Copies the exact 256-byte request into service-owned storage, clears the 1,040-byte response,
 * and selects the standard or checked command family. An active transfer is left unchanged.
 *
 * @param[in,out] service Transfer service state.
 * @param[in] request Complete request payload.
 * @param[in] request_length Number of request bytes.
 * @param[in] checked True to use checked accessory commands and response integrity.
 * @return True when a new transfer starts; otherwise false.
 */
bool i2c_probe_transfer_service_start(I2cProbeTransferService *service, const uint8_t *request,
                                      uint16_t request_length, bool checked) {
    if (service == 0 || request == 0 || request_length != sizeof(service->request) ||
        service->status == I2C_PROBE_TRANSFER_SERVICE_RUNNING) {
        return false;
    }

    memcpy(service->request, request, sizeof(service->request));
    memset(service->response, 0, sizeof(service->response));
    i2c_probe_transfer_sequence_init(&service->sequence, checked);
    i2c_probe_exchange_service_init(&service->exchange);
    service->status = I2C_PROBE_TRANSFER_SERVICE_RUNNING;
    service->result = I2C_PROBE_EXCHANGE_PENDING;
    return true;
}

/**
 * @brief Starts the exchange for the current transfer step.
 *
 * Encodes the step's phase, fragment index, length, and optional request slice, then submits it to
 * the asynchronous command-exchange service.
 *
 * @param[in,out] service Running transfer service.
 * @return True when the current exchange starts; otherwise false.
 */
static bool start_current_exchange(I2cProbeTransferService *service) {
    if (!i2c_probe_transfer_sequence_current(&service->sequence, &service->current_step)) {
        return false;
    }

    service->current_input = (I2cProbeTransferInput){
        .phase = service->current_step.phase,
        .chunk_index = service->current_step.chunk_index,
        .chunk_length = service->current_step.chunk_length,
    };
    if (service->sequence.stage == I2C_PROBE_TRANSFER_WRITING) {
        service->current_input.chunk = service->request + service->current_step.buffer_offset;
    }

    return i2c_probe_transfer_encode(service->current_step.command, &service->current_input,
                                     &service->exchange.frame) &&
           i2c_probe_exchange_service_start(&service->exchange, &service->exchange.frame);
}

/**
 * @brief Applies one completed command exchange.
 *
 * Copies a read payload into its response-buffer offset, advances the transfer sequence, and marks
 * the service complete after the finish exchange.
 *
 * @param[in,out] service Running transfer service with a completed exchange.
 * @return True when the completed exchange matches the current step; otherwise false.
 */
static bool complete_current_exchange(I2cProbeTransferService *service) {
    if (!i2c_probe_transfer_sequence_current(&service->sequence, &service->current_step)) {
        return false;
    }

    if (service->sequence.stage == I2C_PROBE_TRANSFER_READING) {
        const uint8_t *payload = i2c_probe_exchange_service_payload(
            &service->exchange, &service->current_payload_length);
        if (payload == 0 || service->current_payload_length != service->current_step.chunk_length ||
            (uint32_t)service->current_step.buffer_offset + service->current_payload_length >
                sizeof(service->response)) {
            return false;
        }
        memcpy(service->response + service->current_step.buffer_offset, payload,
               service->current_payload_length);
    }

    if (!i2c_probe_transfer_sequence_accept(&service->sequence)) {
        return false;
    }
    if (service->sequence.stage == I2C_PROBE_TRANSFER_COMPLETE) {
        service->status = I2C_PROBE_TRANSFER_SERVICE_COMPLETE;
        service->result = I2C_PROBE_EXCHANGE_SUCCEEDED;
    }
    return true;
}

/**
 * @brief Advances an accessory data transfer.
 *
 * Services the active command exchange, copies completed read fragments, starts the next sequence
 * step, and preserves command, checksum, and response failures from the exchange layer.
 *
 * @param[in,out] service Transfer service state and owned buffers.
 */
void i2c_probe_transfer_service_run(I2cProbeTransferService *service) {
    if (service == 0 || service->status != I2C_PROBE_TRANSFER_SERVICE_RUNNING) {
        return;
    }

    I2cProbeExchangeServiceStatus exchange_status =
        i2c_probe_exchange_service_status(&service->exchange);
    if (exchange_status == I2C_PROBE_EXCHANGE_SERVICE_RUNNING) {
        i2c_probe_exchange_service_run(&service->exchange);
        return;
    }
    if (exchange_status == I2C_PROBE_EXCHANGE_SERVICE_FAILED) {
        service->status = I2C_PROBE_TRANSFER_SERVICE_FAILED;
        service->result = i2c_probe_exchange_service_result(&service->exchange);
        return;
    }
    if (exchange_status == I2C_PROBE_EXCHANGE_SERVICE_COMPLETE &&
        !complete_current_exchange(service)) {
        service->status = I2C_PROBE_TRANSFER_SERVICE_FAILED;
        service->result = I2C_PROBE_EXCHANGE_RESPONSE_ERROR;
        return;
    }
    if (service->status == I2C_PROBE_TRANSFER_SERVICE_RUNNING && !start_current_exchange(service)) {
        service->status = I2C_PROBE_TRANSFER_SERVICE_FAILED;
        service->result = I2C_PROBE_EXCHANGE_RESPONSE_ERROR;
    }
}

/**
 * @brief Returns the accessory data-transfer status.
 *
 * Reports whether the service is dormant, running, complete, or failed.
 *
 * @param[in] service Transfer service state.
 * @return Current status, or idle for a null service.
 */
I2cProbeTransferServiceStatus
i2c_probe_transfer_service_status(const I2cProbeTransferService *service) {
    return service == 0 ? I2C_PROBE_TRANSFER_SERVICE_IDLE : service->status;
}

/**
 * @brief Returns the accessory data-transfer result.
 *
 * Exposes the pending, success, command, checksum, or response classification from the active or
 * most recently completed transfer.
 *
 * @param[in] service Transfer service state.
 * @return Current result, or pending for a null service.
 */
I2cProbeExchangeResult i2c_probe_transfer_service_result(const I2cProbeTransferService *service) {
    return service == 0 ? I2C_PROBE_EXCHANGE_PENDING : service->result;
}

/**
 * @brief Returns the completed accessory response.
 *
 * Exposes the service-owned 1,040-byte response only after every read and the finish exchange have
 * completed successfully.
 *
 * @param[in] service Transfer service state and response storage.
 * @param[out] length Response length when the transfer completed.
 * @return Completed response bytes; otherwise null.
 */
const uint8_t *i2c_probe_transfer_service_response(const I2cProbeTransferService *service,
                                                   uint16_t *length) {
    if (service == 0 || length == 0 || service->status != I2C_PROBE_TRANSFER_SERVICE_COMPLETE) {
        return 0;
    }
    *length = sizeof(service->response);
    return service->response;
}
