#ifndef OPENTEC_BASE_A71CH_AUTHENTICATION_SERVICE_H
#define OPENTEC_BASE_A71CH_AUTHENTICATION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"
#include "secure_element/exchange.h"

/** @brief Lifecycle status of the authentication service. */
typedef enum {
    A71CH_AUTHENTICATION_SERVICE_IDLE,     /**< No authentication exchange is active. */
    A71CH_AUTHENTICATION_SERVICE_RUNNING,  /**< An authentication exchange is in progress. */
    A71CH_AUTHENTICATION_SERVICE_COMPLETE, /**< The authentication exchange completed successfully.
                                            */
    A71CH_AUTHENTICATION_SERVICE_FAILED,   /**< Local exchange setup failed and needs session
                                                recovery. */
} A71chAuthenticationServiceStatus;

/** @brief State and storage for one authentication exchange. */
typedef struct {
    A71chAuthenticationSequence sequence; /**< Multi-fragment authentication sequence state. */
    A71chExchangeService exchange;        /**< Current asynchronous APDU exchange state. */
    A71chAuthenticationStep current_step; /**< Description of the current authentication step. */
    A71chAuthenticationInput
        current_input; /**< Input metadata for the current authentication step. */
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE]; /**< Service-owned authentication request
                                                         buffer. */
    uint8_t response[A71CH_AUTHENTICATION_READ_SIZE]; /**< Service-owned authentication response
                                                         buffer. */
    uint8_t current_payload_length; /**< Number of payload bytes returned by the current read
                                       exchange. */
    bool finish_recovery_pending; /**< A response failure has scheduled the official finish step
                                    before restarting the transfer. */
    A71chAuthenticationServiceStatus status; /**< Current service lifecycle status. */
} A71chAuthenticationService;

/**
 * @brief Initializes the authentication service.
 *
 * Clears service-owned buffers and resets the sequence, exchange, and status state.
 *
 * @param[out] service Service state to initialize.
 */
void a71ch_authentication_service_init(A71chAuthenticationService *service);

/**
 * @brief Starts an authentication exchange.
 *
 * Copies a complete request into service-owned storage and selects plain or LRC protocol commands.
 * A fresh session initializes its exchange and performs the normal readiness poll before its first
 * command.
 *
 * @param[in,out] service Service state that is not currently running.
 * @param[in] request Authentication request bytes.
 * @param[in] request_length Number of request bytes; must equal A71CH_AUTHENTICATION_WRITE_SIZE.
 * @param[in] use_lrc True to use LRC command variants; false to use plain variants.
 * @return True when the request is accepted and the exchange enters the running state; otherwise
 * false.
 */
bool a71ch_authentication_service_start(A71chAuthenticationService *service, const uint8_t *request,
                                        uint16_t request_length, bool use_lrc);

/**
 * @brief Advances the authentication exchange.
 *
 * Services one asynchronous APDU step and starts subsequent steps until the exchange completes.
 * Command, integrity, and response failures follow the secure element's internal startup or finish
 * recovery path. The exchange command queue is preserved across protocol failures so recovery
 * starts with a command write instead of another readiness poll. Only an unrecoverable local
 * encoding or state error enters the failed status.
 *
 * @param[in,out] service Running authentication service state.
 */
void a71ch_authentication_service_run(A71chAuthenticationService *service);

/**
 * @brief Returns the authentication service status.
 *
 * Reports the lifecycle state of service without changing the exchange.
 *
 * @param[in] service Service state to inspect.
 * @return Current service status, or idle when service is null.
 */
A71chAuthenticationServiceStatus
a71ch_authentication_service_status(const A71chAuthenticationService *service);

/**
 * @brief Returns the completed authentication response.
 *
 * Provides the service-owned response buffer only after the authentication exchange completes
 * successfully.
 *
 * @param[in] service Service state to inspect.
 * @return Pointer to the complete response buffer, or null before successful completion.
 */
const uint8_t *a71ch_authentication_service_response(const A71chAuthenticationService *service);

#endif
