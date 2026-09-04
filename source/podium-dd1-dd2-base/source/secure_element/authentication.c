#include "secure_element/authentication.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "secure_element/a71ch.h"
#include "secure_element/exchange.h"

/**
 * @brief Initializes the A71CH authentication service.
 *
 * Clears the owned request and response buffers, command sequence, exchange state, and recovery
 * state.
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
 * and selects plain or SCI2C LRC commands. A fresh session initializes its exchange and performs
 * the normal readiness poll; an active exchange is left unchanged.
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
    service->finish_recovery_pending = false;
    return true;
}

/**
 * @brief Restarts the authentication sequence from its first upload fragment.
 *
 * Preserves the request and protocol variant while clearing partial response data and the current
 * APDU state. A failed exchange keeps its command-queue stage so the exchange owner can submit the
 * recovery command without another readiness poll.
 *
 * @param[in,out] service Authentication service state to restart.
 */
static void restart_authentication_sequence(A71chAuthenticationService *service) {
    bool use_lrc = service->sequence.use_lrc;

    a71ch_authentication_sequence_init(&service->sequence, use_lrc);
    memset(service->response, 0, sizeof(service->response));
    service->current_payload_length = 0;
    service->finish_recovery_pending = false;
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
        if (service->finish_recovery_pending) {
            restart_authentication_sequence(service);
        } else {
            service->status = A71CH_AUTHENTICATION_SERVICE_COMPLETE;
        }
    }
    return true;
}

/**
 * @brief Restarts the official authentication recovery path for a failed APDU.
 *
 * Command and integrity failures restart at the first upload fragment. A malformed device response
 * first sends the finish command, then restarts the upload after that command succeeds. The
 * exchange service owns the failed command queue and is deliberately not reinitialized here.
 *
 * @param[in,out] service Authentication service with the failed APDU.
 * @param[in] result Result reported by the failed exchange.
 * @return True when the recovery APDU was started; otherwise false.
 */
static bool recover_authentication(A71chAuthenticationService *service,
                                   A71chExchangeResult result) {
    if (result == A71CH_EXCHANGE_RESPONSE_ERROR) {
        service->sequence.stage = A71CH_AUTHENTICATION_FINISHING;
        service->sequence.chunk_index = 0;
        service->current_payload_length = 0;
        service->finish_recovery_pending = true;
    } else {
        restart_authentication_sequence(service);
    }

    return start_current_exchange(service);
}

/**
 * @brief Marks an unrecoverable local authentication error.
 *
 * Protocol failures are handled by recover_authentication(). This status is reserved for an
 * invalid local sequence or an encoded frame that cannot be submitted.
 *
 * @param[in,out] service Authentication service state to mark failed.
 */
static void fail_authentication(A71chAuthenticationService *service) {
    service->status = A71CH_AUTHENTICATION_SERVICE_FAILED;
}

/**
 * @brief Advances the A71CH authentication exchange.
 *
 * Services the active APDU, copies retrieved fragments, starts the next step, and routes protocol
 * failures through the device's startup or finish recovery path. Only local setup failures stop the
 * service.
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
        if (!recover_authentication(service, a71ch_exchange_service_result(&service->exchange))) {
            fail_authentication(service);
        }
        return;
    }
    if (exchange_status == A71CH_EXCHANGE_SERVICE_COMPLETE && !complete_current_exchange(service)) {
        if (!recover_authentication(service, A71CH_EXCHANGE_RESPONSE_ERROR)) {
            fail_authentication(service);
        }
        return;
    }
    if (service->status == A71CH_AUTHENTICATION_SERVICE_RUNNING &&
        !start_current_exchange(service)) {
        fail_authentication(service);
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
