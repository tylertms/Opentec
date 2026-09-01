#ifndef OPENTEC_BASE_A71CH_SESSION_SERVICE_H
#define OPENTEC_BASE_A71CH_SESSION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"

/** @brief Lifecycle status of the A71CH session service. */
typedef enum {
    A71CH_SESSION_SERVICE_IDLE,     /**< No session initialization is active. */
    A71CH_SESSION_SERVICE_RUNNING,  /**< Session initialization is in progress. */
    A71CH_SESSION_SERVICE_COMPLETE, /**< Session initialization completed successfully. */
} A71chSessionServiceStatus;

/** @brief Session response storage sizing constants. */
enum {
    A71CH_SESSION_RESPONSE_CAPACITY = 0x1f, /**< Maximum response bytes stored by the service. */
};

/** @brief State and storage for A71CH session initialization. */
typedef struct {
    A71chSession session;        /**< Protocol session state machine. */
    A71chCommand active_command; /**< Command currently owned by the auxiliary bus. */
    uint8_t
        response[A71CH_SESSION_RESPONSE_CAPACITY]; /**< Service-owned command response storage. */
    A71chSessionResponse response_view; /**< Parsed view of the active command response. */
    A71chSessionServiceStatus status;   /**< Service lifecycle status. */
    bool transfer_active;               /**< Whether an auxiliary-bus transaction is active. */
} A71chSessionService;

/**
 * @brief Initializes the A71CH session service.
 *
 * Clears service-owned state and leaves the service idle until started.
 *
 * @param[out] service Service state to initialize.
 */
void a71ch_session_service_init(A71chSessionService *service);

/**
 * @brief Starts session initialization.
 *
 * Initializes a new protocol sequence unless one is already running, which remains unchanged.
 *
 * @param[in,out] service Service state that is not currently running.
 */
void a71ch_session_service_start(A71chSessionService *service);

/**
 * @brief Advances session initialization.
 *
 * Completes an active bus operation or starts the command required by the current protocol stage.
 *
 * @param[in,out] service Running session service state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void a71ch_session_service_run(A71chSessionService *service, uint32_t now_ms);

/**
 * @brief Returns the session service status.
 *
 * Reports whether session initialization is idle, running, or complete.
 *
 * @param[in] service Service state to inspect.
 * @return Current service status.
 */
A71chSessionServiceStatus a71ch_session_service_status(const A71chSessionService *service);

#endif
