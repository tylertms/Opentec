#ifndef OPENTEC_BASE_SERIAL_SERVICE_H
#define OPENTEC_BASE_SERIAL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/session.h"

/** @brief Lifecycle status of the serial request service. */
typedef enum {
    SERIAL_SERVICE_IDLE,      /**< No logical request is active. */
    SERIAL_SERVICE_PENDING,   /**< A request is awaiting packet exchanges. */
    SERIAL_SERVICE_SUCCEEDED, /**< A matching response completed successfully. */
    SERIAL_SERVICE_FAILED,    /**< The request or transport exchange failed. */
} SerialServiceStatus;

/** @brief State and storage for one serial request exchange. */
typedef struct {
    SerialSession session;              /**< Shared packet session state. */
    SerialMessageAssembly response;     /**< Dedicated response-message assembly state. */
    uint8_t packet[SERIAL_PACKET_SIZE]; /**< Service-owned packet transfer buffer. */
    uint32_t deadline_ms;               /**< Deadline for the active packet response. */
    uint32_t error_count;               /**< Cumulative packet and timeout errors. */
    uint8_t request_type;               /**< Logical type of the request being serviced. */
    uint8_t attempts;                   /**< Number of timeout or malformed-packet retries. */
    SerialServiceStatus status;         /**< Current service lifecycle status. */
    bool packet_pending;                /**< Whether packet is active on the physical link. */
    bool recovery_pending;              /**< Whether a synchronization packet awaits recovery expiry. */
    bool bounded_attempts;              /**< True when retries are limited to five. */
} SerialService;

/**
 * @brief Initializes the serial request service.
 *
 * Clears service state and initializes its packet session without starting a physical transfer.
 *
 * @param[out] service Service state to initialize.
 */
void serial_service_init(SerialService *service);

/**
 * @brief Starts one logical serial request.
 *
 * Queues a request message and starts its first packet exchange on the shared serial link. Timeout
 * and malformed-packet retries continue without a retry-count failure.
 *
 * @param[in,out] service Idle service state accepting the request.
 * @param[in] type Logical request type from SERIAL_MESSAGE_FIRST_TYPE through
 * SERIAL_MESSAGE_LAST_TYPE.
 * @param[in] message Complete request message bytes.
 * @param[in] length Request length from one through SERIAL_MESSAGE_MAX_SIZE bytes.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the request and first packet exchange start; otherwise false.
 */
bool serial_service_start(SerialService *service, uint8_t type, const uint8_t *message,
                          uint16_t length, uint32_t now_ms);

/**
 * @brief Starts a serial request with bounded transport retries.
 *
 * The request retries timeout and malformed packets through the fifth retry, then reports failure.
 *
 * @param[in,out] service Idle service state accepting the request.
 * @param[in] type Logical request type.
 * @param[in] message Complete request message bytes.
 * @param[in] length Request length in bytes.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the request starts; otherwise false.
 */
bool serial_service_start_wait(SerialService *service, uint8_t type, const uint8_t *message,
                               uint16_t length, uint32_t now_ms);

/**
 * @brief Advances the active serial request.
 *
 * Processes received packets, starts required follow-up packets, and retries timed-out packets.
 *
 * @param[in,out] service Pending service state to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void serial_service_run(SerialService *service, uint32_t now_ms);

/**
 * @brief Returns the completed serial response.
 *
 * Provides the assembled response only after a matching request completes successfully.
 *
 * @param[in] service Service state to inspect.
 * @return Completed response assembly, or null when no successful response is available.
 */
const SerialMessageAssembly *serial_service_response(const SerialService *service);

/**
 * @brief Returns the cumulative serial error count.
 *
 * Reports rejected or overflowing packet and response-timeout errors recorded by the service.
 *
 * @param[in] service Service state to inspect.
 * @return Error count, or zero when service is null.
 */
uint32_t serial_service_error_count(const SerialService *service);

/**
 * @brief Cancels the current attached-device request.
 *
 * Stops a pending physical transfer, clears logical transmit and receive state, and returns the
 * service to idle while preserving the shared packet sequence and cumulative error count.
 *
 * @param[in,out] service Serial service to cancel.
 */
void serial_service_cancel(SerialService *service);

/**
 * @brief Releases a finished serial request.
 *
 * Consumes any completed response and resets request state while preserving packet sequence state.
 *
 * @param[in,out] service Completed or failed service state to release.
 */
void serial_service_release(SerialService *service);

#endif
