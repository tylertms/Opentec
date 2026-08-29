#include "secure_element/exchange.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/aux_bus.h"
#include "secure_element/a71ch.h"
#include "secure_element/bus.h"

/**
 * @brief Initializes the asynchronous A71CH APDU exchange service.
 *
 * Clears command, response, protocol, and auxiliary-bus ownership state.
 *
 * @param[out] service Exchange service state.
 */
void a71ch_exchange_service_init(A71chExchangeService *service) {
    *service = (A71chExchangeService){0};
}

/**
 * @brief Starts one encoded A71CH APDU exchange.
 *
 * Copies the command frame and begins with an SCI2C status poll. An active exchange is left
 * unchanged.
 *
 * @param[in,out] service Exchange service state.
 * @param[in] frame Encoded write command and expected response layout.
 * @return True when a new exchange starts; otherwise false.
 */
bool a71ch_exchange_service_start(A71chExchangeService *service,
                                  const A71chAuthenticationFrame *frame) {
    if (service == 0 || frame == 0 || frame->write_length == 0 || frame->response_length == 0 ||
        frame->response_length > sizeof(service->response) ||
        service->status == A71CH_EXCHANGE_SERVICE_RUNNING) {
        return false;
    }

    service->frame = *frame;
    memset(service->status_response, 0, sizeof(service->status_response));
    memset(service->response, 0, sizeof(service->response));
    service->parsed_response = (A71chAuthenticationResponse){0};
    a71ch_exchange_init(&service->exchange);
    service->status = A71CH_EXCHANGE_SERVICE_RUNNING;
    service->transfer_active = false;
    return true;
}

/**
 * @brief Applies one completed auxiliary-bus operation to the command exchange.
 *
 * Releases the bus result, retries failed operations at the same protocol stage, evaluates status
 * responses, records a queued APDU write, or validates the final response.
 *
 * @param[in,out] service Active exchange service state and response buffers.
 * @param[in] succeeded True when the auxiliary-bus operation succeeded.
 */
static void complete_transfer(A71chExchangeService *service, bool succeeded) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }

    if (service->exchange.stage == A71CH_EXCHANGE_WAIT_READY ||
        service->exchange.stage == A71CH_EXCHANGE_WAIT_ACCEPTANCE) {
        a71ch_exchange_status(&service->exchange, service->status_response[1]);
    } else if (service->exchange.stage == A71CH_EXCHANGE_QUEUE_COMMAND) {
        a71ch_exchange_command_queued(&service->exchange);
    } else if (service->exchange.stage == A71CH_EXCHANGE_WAIT_RESPONSE) {
        A71chFrameValidation validation = a71ch_authentication_response_parse(
            &service->frame, service->response, service->frame.response_length,
            &service->parsed_response);
        a71ch_exchange_finalize(&service->exchange, validation);
        service->status = service->exchange.stage == A71CH_EXCHANGE_COMPLETE
                              ? A71CH_EXCHANGE_SERVICE_COMPLETE
                              : A71CH_EXCHANGE_SERVICE_FAILED;
    }

    if (service->exchange.stage == A71CH_EXCHANGE_FAILED) {
        service->status = A71CH_EXCHANGE_SERVICE_FAILED;
    }
}

/**
 * @brief Advances one asynchronous A71CH APDU exchange.
 *
 * Services one owned transaction or, while the shared bus is idle, starts the ready poll, command
 * write, acceptance poll, or final response read selected by the protocol stage.
 *
 * @param[in,out] service Exchange service state and response buffers.
 */
void a71ch_exchange_service_run(A71chExchangeService *service) {
    if (service == 0 || service->status != A71CH_EXCHANGE_SERVICE_RUNNING) {
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

    if (service->exchange.stage == A71CH_EXCHANGE_WAIT_READY ||
        service->exchange.stage == A71CH_EXCHANGE_WAIT_ACCEPTANCE) {
        service->transfer_active = a71ch_bus_start(A71CH_READ_STATUS, service->status_response);
    } else if (service->exchange.stage == A71CH_EXCHANGE_QUEUE_COMMAND) {
        service->transfer_active = a71ch_bus_start_frame_write(&service->frame);
    } else if (service->exchange.stage == A71CH_EXCHANGE_WAIT_RESPONSE) {
        service->transfer_active = a71ch_bus_start_frame_read(&service->frame, service->response);
    }
}

/**
 * @brief Returns a completed A71CH response payload.
 *
 * Makes a parsed read payload available after the command exchange completes. Write and finish
 * responses complete without a payload.
 *
 * @param[in] service Exchange service state and response buffer.
 * @param[out] length Payload length when a read response completed.
 * @return Read payload after successful completion; otherwise null.
 */
const uint8_t *a71ch_exchange_service_payload(const A71chExchangeService *service,
                                              uint8_t *length) {
    if (service == 0 || length == 0 || service->status != A71CH_EXCHANGE_SERVICE_COMPLETE ||
        service->parsed_response.payload == 0) {
        return 0;
    }
    *length = service->parsed_response.payload_length;
    return service->parsed_response.payload;
}

/**
 * @brief Returns the A71CH APDU exchange service status.
 *
 * Reports whether the service is dormant, transferring, complete, or failed.
 *
 * @param[in] service Exchange service state.
 * @return Current service status, or idle for a null service.
 */
A71chExchangeServiceStatus a71ch_exchange_service_status(const A71chExchangeService *service) {
    return service == 0 ? A71CH_EXCHANGE_SERVICE_IDLE : service->status;
}

/**
 * @brief Returns the A71CH APDU exchange result.
 *
 * Exposes the pending, success, command, LRC, or response classification from the protocol
 * exchange.
 *
 * @param[in] service Exchange service state.
 * @return Current protocol result, or pending for a null service.
 */
A71chExchangeResult a71ch_exchange_service_result(const A71chExchangeService *service) {
    return service == 0 ? A71CH_EXCHANGE_PENDING : service->exchange.result;
}
