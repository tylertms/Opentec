#ifndef OPENTEC_BASE_WHEEL_TRANSFER_SERVICE_H
#define OPENTEC_BASE_WHEEL_TRANSFER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Fixed payload size used by the wheel-transfer handshake. */
enum {
    WHEEL_TRANSFER_PAYLOAD_SIZE = 10, /**< Number of bytes written and read by the handshake. */
    WHEEL_TRANSFER_NATIVE_PAYLOAD_CAPACITY = 124, /**< Maximum native transfer payload size. */
    WHEEL_TRANSFER_NATIVE_QUEUE_CAPACITY = 4,     /**< Native payloads retained for the wheel. */
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
    WHEEL_TRANSFER_READ_FAILED = -2,      /**< The selected transport rejected the read. */
    WHEEL_TRANSFER_WRITE_FAILED = -1,     /**< The selected transport rejected the write. */
    WHEEL_TRANSFER_IDLE = 0,              /**< No request is active and no result is available. */
    WHEEL_TRANSFER_PENDING = 1,           /**< The request has started but is not complete. */
    WHEEL_TRANSFER_COMPLETE = 2,          /**< The request completed with a valid response. */
} WheelTransferStatus;

/** @brief Internal phases of one wheel-transfer handshake. */
typedef enum {
    WHEEL_TRANSFER_PHASE_IDLE,          /**< No transfer is active. */
    WHEEL_TRANSFER_PHASE_WRITE_READY,   /**< The probe write is ready to queue. */
    WHEEL_TRANSFER_PHASE_WRITE_PENDING, /**< The probe write is in progress or complete. */
    WHEEL_TRANSFER_PHASE_READ_READY,    /**< The response read is ready to queue. */
    WHEEL_TRANSFER_PHASE_READ_PENDING,  /**< The response read is in progress or complete. */
} WheelTransferPhase;

/** @brief One complete native attached-wheel transfer payload. */
typedef struct {
    uint8_t data[WHEEL_TRANSFER_NATIVE_PAYLOAD_CAPACITY]; /**< Payload bytes. */
    uint8_t length;                                       /**< Number of valid payload bytes. */
} WheelTransferNativePayload;

/** @brief State and results for the wheel-transfer handshake service. */
typedef struct {
    uint8_t response[WHEEL_TRANSFER_PAYLOAD_SIZE]; /**< Response bytes retained for validation. */
    WheelTransferStatus
        statuses[WHEEL_TRANSFER_REQUEST_COUNT]; /**< Last status per request channel. */
    WheelTransferRequest
        request; /**< Selected request channel for the current or most recent transfer. */
    WheelTransferPhase phase; /**< Current handshake phase. */
    WheelTransferNativePayload native_queue[WHEEL_TRANSFER_NATIVE_QUEUE_CAPACITY];
    uint8_t native_queue_read;  /**< Next native payload to consume. */
    uint8_t native_queue_write; /**< Next native payload slot to fill. */
    uint8_t native_queue_count; /**< Number of queued native payloads. */
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
 * @param[in] request Write or read channel. Negative and out-of-range values are rejected.
 * @return True when the service accepted a supported request; otherwise false.
 */
bool wheel_transfer_service_start(WheelTransferService *service, WheelTransferRequest request);

/**
 * @brief Advances the active wheel-transfer handshake.
 *
 * Claims the request owner, processes the fixed probe write and response read, and stores the final
 * transfer or checksum status. The write-channel request uses the shared auxiliary bus; the
 * read-channel request uses the shared command transport.
 *
 * @param[in,out] service Wheel-transfer service to advance.
 * @param[in,out] transport Shared command transport used by the read-channel handshake.
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

/**
 * @brief Queues one complete native wheel-transfer payload.
 *
 * Retains a complete payload after the USB fragment reassembler has validated it. The queue is
 * consumed by the attached-wheel transfer owner and is independent from the ten-byte handshake.
 *
 * @param[in,out] service Wheel-transfer service receiving the payload.
 * @param[in] payload Complete native payload bytes.
 * @param[in] length Payload length from one through 124 bytes.
 * @return True when the payload was retained; otherwise false.
 */
bool wheel_transfer_service_queue_native_payload(WheelTransferService *service,
                                                 const uint8_t *payload, uint8_t length);

/**
 * @brief Returns the oldest queued native wheel-transfer payload.
 *
 * The returned view remains valid until wheel_transfer_service_release_native_payload() is called.
 *
 * @param[in] service Wheel-transfer service to inspect.
 * @return Oldest queued payload, or NULL when the queue is empty.
 */
const WheelTransferNativePayload *
wheel_transfer_service_native_payload(const WheelTransferService *service);

/**
 * @brief Releases the oldest native wheel-transfer payload.
 *
 * Advances the queue after the attached-wheel transfer owner has consumed the returned payload.
 *
 * @param[in,out] service Wheel-transfer service to update.
 */
void wheel_transfer_service_release_native_payload(WheelTransferService *service);

#endif
