#ifndef OPENTEC_BASE_A71CH_EXCHANGE_SERVICE_H
#define OPENTEC_BASE_A71CH_EXCHANGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"

/** @brief Lifecycle status of the asynchronous exchange service. */
typedef enum {
    A71CH_EXCHANGE_SERVICE_IDLE,     /**< No exchange is active. */
    A71CH_EXCHANGE_SERVICE_RUNNING,  /**< An exchange is in progress. */
    A71CH_EXCHANGE_SERVICE_COMPLETE, /**< The exchange completed successfully. */
    A71CH_EXCHANGE_SERVICE_FAILED,   /**< The exchange failed. */
} A71chExchangeServiceStatus;

/** @brief Exchange response storage sizing constants. */
enum {
    A71CH_EXCHANGE_RESPONSE_CAPACITY =
        A71CH_AUTHENTICATION_CHUNK_CAPACITY +
        4, /**< Maximum response bytes stored by the exchange service. */
};

/** @brief State and storage for one asynchronous APDU exchange. */
typedef struct {
    A71chExchange exchange;         /**< Protocol-level exchange state. */
    A71chAuthenticationFrame frame; /**< Encoded command frame being exchanged. */
    uint8_t status_response[2];     /**< Response bytes returned by status polling. */
    uint8_t response[A71CH_EXCHANGE_RESPONSE_CAPACITY]; /**< Service-owned APDU response storage. */
    A71chAuthenticationResponse parsed_response; /**< Parsed view of the APDU response payload. */
    A71chExchangeServiceStatus status;           /**< Service lifecycle status. */
    bool transfer_active; /**< Whether an auxiliary-bus operation is active. */
} A71chExchangeService;

/**
 * @brief Initializes the asynchronous exchange service.
 *
 * Clears command, response, protocol, and auxiliary-bus ownership state.
 *
 * @param[out] service Service state to initialize.
 */
void a71ch_exchange_service_init(A71chExchangeService *service);

/**
 * @brief Starts an encoded APDU exchange.
 *
 * Copies frame into service-owned storage and schedules the initial readiness status poll.
 *
 * @param[in,out] service Service state that is not currently running.
 * @param[in] frame Encoded command frame and expected response layout.
 * @return True when the exchange enters the running state; otherwise false.
 */
bool a71ch_exchange_service_start(A71chExchangeService *service,
                                  const A71chAuthenticationFrame *frame);

/**
 * @brief Advances the asynchronous APDU exchange.
 *
 * Completes an active bus operation or starts the operation required by the current protocol stage.
 *
 * @param[in,out] service Running exchange service state.
 */
void a71ch_exchange_service_run(A71chExchangeService *service);

/**
 * @brief Returns the completed APDU payload.
 *
 * Provides a view into service-owned response storage after a successful read exchange.
 *
 * @param[in] service Service state to inspect.
 * @param[out] length Destination for payload length.
 * @return Pointer to the payload when available; otherwise null.
 */
const uint8_t *a71ch_exchange_service_payload(const A71chExchangeService *service, uint8_t *length);

/**
 * @brief Returns the exchange service status.
 *
 * Reports the lifecycle state without changing the exchange.
 *
 * @param[in] service Service state to inspect.
 * @return Current service status, or idle when service is null.
 */
A71chExchangeServiceStatus a71ch_exchange_service_status(const A71chExchangeService *service);

/**
 * @brief Returns the protocol exchange result.
 *
 * Exposes the pending, success, command-error, LRC-error, or response-error classification.
 *
 * @param[in] service Service state to inspect.
 * @return Current protocol result, or pending when service is null.
 */
A71chExchangeResult a71ch_exchange_service_result(const A71chExchangeService *service);

#endif
