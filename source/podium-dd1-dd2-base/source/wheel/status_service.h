#ifndef OPENTEC_BASE_WHEEL_STATUS_SERVICE_H
#define OPENTEC_BASE_WHEEL_STATUS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/service.h"

/** @brief Decoded status values from the attached wheel's type-five response. */
typedef struct {
    uint8_t status_high;      /**< High status byte from the latest response. */
    uint8_t status_low;       /**< Low status byte from the latest response. */
    uint16_t accessory_value; /**< Little-endian accessory value from the latest response. */
    uint32_t
        runtime_seconds; /**< Little-endian runtime-seconds counter from the latest response. */
    uint32_t runtime_counter; /**< Little-endian runtime counter from the latest response. */
    uint8_t trailing_status;  /**< Trailing status byte from the latest response. */
} WheelStatusSnapshot;

/** @brief Polling state for attached-wheel status requests. */
typedef struct {
    SerialService *transport;     /**< Shared serial service used for type-five requests. */
    WheelStatusSnapshot snapshot; /**< Most recently decoded status response. */
    uint32_t next_poll_ms;        /**< Deadline compared with strict unsigned time ordering. */
    uint8_t request_marker;       /**< Marker byte for the next request, normally zero or 0xAA. */
    bool marked_response_ready;   /**< Whether a response ending in marker 0xAA is latched. */
} WheelStatusService;

/** @brief State of the bounded startup status transaction. */
typedef enum {
    WHEEL_STATUS_STARTUP_QUEUE = 0,    /**< Request has not claimed the serial transport. */
    WHEEL_STATUS_STARTUP_WAIT = 1,     /**< Bounded serial exchange is active. */
    WHEEL_STATUS_STARTUP_COMPLETE = 2, /**< Matching type-five response completed. */
    WHEEL_STATUS_STARTUP_FAILED = 3,   /**< Transport start or bounded retries failed. */
} WheelStatusStartupState;

/** @brief Local state for the bounded startup status transaction. */
typedef struct {
    WheelStatusService *service;   /**< Status service that owns the request. */
    WheelStatusStartupState state; /**< Current official transaction state. */
} WheelStatusStartupTransaction;

/**
 * @brief Initializes attached-wheel status polling.
 *
 * Clears the snapshot and polling state, then stores the shared serial transport pointer.
 *
 * @param[out] service Status service to initialize; null is ignored.
 * @param[in,out] transport Shared serial service used for type-five requests.
 */
void wheel_status_service_init(WheelStatusService *service, SerialService *transport);

/**
 * @brief Advances attached-wheel status polling.
 *
 * Consumes a completed response, releases the shared transport, and starts a due one-byte status
 * request when scheduling permits it.
 *
 * @param[in,out] service Status service to advance; null is ignored.
 * @param[in] start_allowed Whether a new request may claim the shared serial service.
 */
void wheel_status_service_run(WheelStatusService *service, bool start_allowed);

/**
 * @brief Initializes the bounded startup status transaction.
 *
 * Clears the request marker and starts in the queue state used by the official startup transfer.
 *
 * @param[out] transaction Transaction to initialize.
 * @param[in,out] service Status service that owns the shared serial transport.
 */
void wheel_status_startup_transaction_init(WheelStatusStartupTransaction *transaction,
                                           WheelStatusService *service);

/**
 * @brief Advances the bounded startup status transaction.
 *
 * Queues one unmarked type-five request, retains the official one-second periodic deadline, and
 * maps transport completion and retry exhaustion to terminal states two and three.
 *
 * @param[in,out] transaction Transaction to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current startup transaction state.
 */
WheelStatusStartupState
wheel_status_startup_transaction_run(WheelStatusStartupTransaction *transaction, uint32_t now_ms);

/**
 * @brief Marks the next status request.
 *
 * Changes the next request marker to 0xAA without changing the periodic poll deadline.
 *
 * @param[in,out] service Status service to mark; null is ignored.
 */
void wheel_status_service_mark_next_request(WheelStatusService *service);

/**
 * @brief Takes the marked-response notification.
 *
 * Consumes the one-shot notification latched by a response whose final byte is 0xAA.
 *
 * @param[in,out] service Status service to inspect.
 * @return True when a marked response was pending; otherwise false.
 */
bool wheel_status_service_take_marked_response(WheelStatusService *service);

/**
 * @brief Returns the latest status snapshot.
 *
 * Exposes the retained decoded status values without changing the polling state.
 *
 * @param[in] service Status service to inspect.
 * @return Pointer to the snapshot, or null when service is null.
 */
const WheelStatusSnapshot *wheel_status_service_snapshot(const WheelStatusService *service);

/**
 * @brief Reports whether the attached-wheel status exchange is active.
 *
 * Includes pending, completed, and failed status requests until the shared serial service consumes
 * the result.
 *
 * @param[in] service Status service to inspect.
 * @return True while a type-five status exchange owns the serial service; otherwise false.
 */
bool wheel_status_service_exchange_active(const WheelStatusService *service);

#endif
