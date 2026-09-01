#ifndef OPENTEC_BASE_WHEEL_TRANSFER_SERVICE_H
#define OPENTEC_BASE_WHEEL_TRANSFER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Fixed payload size used by the wheel-transfer handshake. */
enum {
    WHEEL_TRANSFER_PAYLOAD_SIZE = 10, /**< Number of bytes written and read by the handshake. */
};

/** @brief Request channels supported by the wheel-transfer handshake. */
typedef enum {
    WHEEL_TRANSFER_WRITE,         /**< Transfer channel used for the write-side handshake. */
    WHEEL_TRANSFER_READ,          /**< Transfer channel used for the read-side handshake. */
    WHEEL_TRANSFER_REQUEST_COUNT, /**< Number of request channels. */
} WheelTransferRequest;

/** @brief Status of one wheel-transfer request channel. */
typedef enum {
    WHEEL_TRANSFER_INVALID_RESPONSE = -3, /**< The response failed validation. */
    WHEEL_TRANSFER_READ_FAILED = -2,      /**< The command transport rejected the read. */
    WHEEL_TRANSFER_WRITE_FAILED = -1,     /**< The command transport rejected the write. */
    WHEEL_TRANSFER_IDLE = 0,              /**< No request is active and no result is available. */
    WHEEL_TRANSFER_PENDING = 1,           /**< The request has started but is not complete. */
    WHEEL_TRANSFER_COMPLETE = 2,          /**< The request completed with a valid response. */
} WheelTransferStatus;

/** @brief Internal phases of one wheel-transfer handshake. */
typedef enum {
    WHEEL_TRANSFER_PHASE_IDLE,          /**< No transfer is active. */
    WHEEL_TRANSFER_PHASE_WRITE_READY,   /**< The probe write is ready to queue. */
    WHEEL_TRANSFER_PHASE_WRITE_PENDING, /**< The probe write is in progress or complete. */
    WHEEL_TRANSFER_PHASE_READ_PENDING,  /**< The response read is in progress or complete. */
} WheelTransferPhase;

/** @brief State and results for the wheel-transfer handshake service. */
typedef struct {
    uint8_t response[WHEEL_TRANSFER_PAYLOAD_SIZE]; /**< Response bytes retained for validation. */
    WheelTransferStatus
        statuses[WHEEL_TRANSFER_REQUEST_COUNT]; /**< Last status per request channel. */
    WheelTransferRequest
        request; /**< Selected request channel for the current or most recent transfer. */
    WheelTransferPhase phase; /**< Current handshake phase. */
} WheelTransferService;

/**
 * @brief Initializes the wheel-transfer service.
 *
 * Clears both request statuses, the response buffer, and the active handshake phase.
 *
 * @param[out] service Wheel-transfer service to initialize.
 */
void wheel_transfer_service_init(WheelTransferService *service);

/**
 * @brief Starts a wheel-transfer handshake.
 *
 * Selects a request channel, marks it pending, clears its response buffer, and schedules the probe
 * write when the service is idle.
 *
 * @param[in,out] service Idle wheel-transfer service to update.
 * @param[in] request Request channel to start.
 * @return True when request is valid and the service accepted it; otherwise false.
 */
bool wheel_transfer_service_start(WheelTransferService *service, WheelTransferRequest request);

/**
 * @brief Advances the active wheel-transfer handshake.
 *
 * Claims the request owner, processes the fixed probe write and response read, and stores the final
 * transfer or checksum status.
 *
 * @param[in,out] service Wheel-transfer service to advance.
 * @param[in,out] transport Shared command transport used by the handshake.
 */
void wheel_transfer_service_run(WheelTransferService *service, CommandTransport *transport);

/**
 * @brief Returns the latest status for a transfer channel.
 *
 * Reads the independent status retained for the requested write or read channel.
 *
 * @param[in] service Wheel-transfer service to inspect.
 * @param[in] request Request channel to inspect.
 * @return Retained channel status, or WHEEL_TRANSFER_IDLE when service or request is invalid.
 */
WheelTransferStatus wheel_transfer_service_status(const WheelTransferService *service,
                                                  WheelTransferRequest request);

#endif
