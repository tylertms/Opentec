#include "secure_element/authentication.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "secure_element/a71ch.h"
#include "secure_element/exchange.h"

/**
 * @brief Initializes the A71CH authentication service.
 *
 * Clears the owned request and response buffers, command sequence, exchange state, and result.
 *
 * @param[out] service Authentication service state.
 */
void a71ch_authentication_service_init(A71chAuthenticationService *service) {
    *service = (A71chAuthenticationService){0};
}

/**
 * @brief Starts an A71CH authentication exchange.
 *
 * Copies the exact 256-byte request into service-owned storage, clears the 1,040-byte response,
 * and selects plain or SCI2C LRC commands. An active exchange is left unchanged.
 *
 * @param[in,out] service Authentication service state.
 * @param[in] request Complete authentication request.
 * @param[in] request_length Number of request bytes.
 * @param[in] use_lrc True to use SCI2C LRC commands.
 * @return True when a new authentication exchange starts; otherwise false.
 */
bool a71ch_authentication_service_start(A71chAuthenticationService *service, const uint8_t *request,
                                        uint16_t request_length, bool use_lrc) {
    if (service == 0 || request == 0 || request_length != sizeof(service->request) ||
        service->status == A71CH_AUTHENTICATION_SERVICE_RUNNING) {
        return false;
    }

    memcpy(service->request, request, sizeof(service->request));
    memset(service->response, 0, sizeof(service->response));
    a71ch_authentication_sequence_init(&service->sequence, use_lrc);
    a71ch_exchange_service_init(&service->exchange);
    service->status = A71CH_AUTHENTICATION_SERVICE_RUNNING;
    service->result = A71CH_EXCHANGE_PENDING;
    return true;
}

/**
 * @brief Starts the exchange for the current authentication step.
 *
 * Encodes the step's phase, fragment index, length, and optional request slice, then submits it to
 * the asynchronous command-exchange service.
 *
 * @param[in,out] service Running authentication service.
 * @return True when the current exchange starts; otherwise false.
 */
static bool start_current_exchange(A71chAuthenticationService *service) {
    if (!a71ch_authentication_sequence_current(&service->sequence, &service->current_step)) {
        return false;
    }

    service->current_input = (A71chAuthenticationInput){
        .phase = service->current_step.phase,
        .chunk_index = service->current_step.chunk_index,
        .chunk_length = service->current_step.chunk_length,
    };
    if (service->sequence.stage == A71CH_AUTHENTICATION_WRITING) {
        service->current_input.chunk = service->request + service->current_step.buffer_offset;
    }

    return a71ch_authentication_encode(service->current_step.command, &service->current_input,
                                       &service->exchange.frame) &&
           a71ch_exchange_service_start(&service->exchange, &service->exchange.frame);
}

/**
 * @brief Applies one completed command exchange.
 *
 * Copies retrieved data into its response offset, advances the sequence, and completes the service
 * after finalization.
 *
 * @param[in,out] service Running authentication service with a completed exchange.
 * @return True when the completed exchange matches the current step; otherwise false.
 */
static bool complete_current_exchange(A71chAuthenticationService *service) {
    if (!a71ch_authentication_sequence_current(&service->sequence, &service->current_step)) {
        return false;
    }

    if (service->sequence.stage == A71CH_AUTHENTICATION_READING) {
        const uint8_t *payload =
            a71ch_exchange_service_payload(&service->exchange, &service->current_payload_length);
        if (payload == 0 || service->current_payload_length != service->current_step.chunk_length ||
            (uint32_t)service->current_step.buffer_offset + service->current_payload_length >
                sizeof(service->response)) {
            return false;
        }
        memcpy(service->response + service->current_step.buffer_offset, payload,
               service->current_payload_length);
    }

    if (!a71ch_authentication_sequence_accept(&service->sequence)) {
        return false;
    }
    if (service->sequence.stage == A71CH_AUTHENTICATION_COMPLETE) {
        service->status = A71CH_AUTHENTICATION_SERVICE_COMPLETE;
        service->result = A71CH_EXCHANGE_SUCCEEDED;
    }
    return true;
}

/**
 * @brief Advances the A71CH authentication exchange.
 *
 * Services the active APDU, copies retrieved fragments, starts the next step, and preserves
 * command, LRC, and response failures from the exchange layer.
 *
 * @param[in,out] service Authentication service state and owned buffers.
 */
void a71ch_authentication_service_run(A71chAuthenticationService *service) {
    if (service == 0 || service->status != A71CH_AUTHENTICATION_SERVICE_RUNNING) {
        return;
    }

    A71chExchangeServiceStatus exchange_status = a71ch_exchange_service_status(&service->exchange);
    if (exchange_status == A71CH_EXCHANGE_SERVICE_RUNNING) {
        a71ch_exchange_service_run(&service->exchange);
        return;
    }
    if (exchange_status == A71CH_EXCHANGE_SERVICE_FAILED) {
        service->status = A71CH_AUTHENTICATION_SERVICE_FAILED;
        service->result = a71ch_exchange_service_result(&service->exchange);
        return;
    }
    if (exchange_status == A71CH_EXCHANGE_SERVICE_COMPLETE && !complete_current_exchange(service)) {
        service->status = A71CH_AUTHENTICATION_SERVICE_FAILED;
        service->result = A71CH_EXCHANGE_RESPONSE_ERROR;
        return;
    }
    if (service->status == A71CH_AUTHENTICATION_SERVICE_RUNNING &&
        !start_current_exchange(service)) {
        service->status = A71CH_AUTHENTICATION_SERVICE_FAILED;
        service->result = A71CH_EXCHANGE_RESPONSE_ERROR;
    }
}

/**
 * @brief Returns the A71CH authentication service status.
 *
 * Reports whether the service is dormant, running, complete, or failed.
 *
 * @param[in] service Authentication service state.
 * @return Current status, or idle for a null service.
 */
A71chAuthenticationServiceStatus
a71ch_authentication_service_status(const A71chAuthenticationService *service) {
    return service == 0 ? A71CH_AUTHENTICATION_SERVICE_IDLE : service->status;
}

/**
 * @brief Returns the A71CH authentication result.
 *
 * Exposes the pending, success, command, LRC, or response classification from the active or
 * most recently completed transfer.
 *
 * @param[in] service Authentication service state.
 * @return Current result, or pending for a null service.
 */
A71chExchangeResult a71ch_authentication_service_result(const A71chAuthenticationService *service) {
    return service == 0 ? A71CH_EXCHANGE_PENDING : service->result;
}

/**
 * @brief Returns the completed A71CH authentication response.
 *
 * Exposes the service-owned 1,040-byte response only after every upload, read, and finalization
 * exchange has completed successfully.
 *
 * @param[in] service Authentication service state and response storage.
 * @return Completed response bytes; otherwise null.
 */
const uint8_t *a71ch_authentication_service_response(const A71chAuthenticationService *service) {
    if (service == 0 || service->status != A71CH_AUTHENTICATION_SERVICE_COMPLETE) {
        return 0;
    }
    return service->response;
}
