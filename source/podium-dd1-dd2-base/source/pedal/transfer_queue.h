#ifndef OPENTEC_BASE_PEDAL_TRANSFER_QUEUE_H
#define OPENTEC_BASE_PEDAL_TRANSFER_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Limits for queued V4 host transfers.
 */
enum {
    PEDAL_TRANSFER_PAYLOAD_CAPACITY = 124, /**< Maximum logical transfer payload size in bytes. */
    PEDAL_TRANSFER_QUEUE_CAPACITY = 11,    /**< Number of logical requests retained by the queue. */
};

/**
 * @brief Stores one complete host request for the V4 pedal controller.
 *
 * Keeps the logical payload independent of its USB and pedal-link framing.
 */
typedef struct {
    uint8_t data[PEDAL_TRANSFER_PAYLOAD_CAPACITY]; /**< Logical host request payload. */
    uint8_t length;                                /**< Number of valid bytes in data. */
} PedalTransferRequest;

/**
 * @brief Retains host pedal requests in arrival order.
 *
 * Tracks the front request separately while it awaits a pedal response and exposes eleven usable
 * request slots.
 */
typedef struct {
    PedalTransferRequest requests[PEDAL_TRANSFER_QUEUE_CAPACITY]; /**< Ring-buffer request slots. */
    uint8_t read_index;  /**< Index of the oldest retained request. */
    uint8_t write_index; /**< Index where the next request is appended. */
    uint8_t count;       /**< Number of retained requests. */
    bool active;         /**< True while the front request awaits a response. */
} PedalTransferQueue;

/**
 * @brief Initializes an empty V4 host-transfer queue.
 *
 * Clears all ring-buffer slots, indices, and active state.
 *
 * @param[out] queue Queue state to initialize.
 */
void pedal_transfer_queue_init(PedalTransferQueue *queue);

/**
 * @brief Appends a host transfer request to the queue.
 *
 * Copies a nonempty payload when the request length and queue capacity are valid.
 *
 * @param[in,out] queue Queue receiving the request.
 * @param[in] data Logical host request payload.
 * @param[in] length Number of payload bytes.
 * @return True when the request was copied into the queue.
 */
bool pedal_transfer_queue_push(PedalTransferQueue *queue, const uint8_t *data, uint8_t length);

/**
 * @brief Returns the front request when it is ready to send.
 *
 * Returns null for an empty queue or while the front request is awaiting a response.
 *
 * @param[in] queue Queue to inspect.
 * @return Read-only front request, or null when no request can be sent.
 */
const PedalTransferRequest *pedal_transfer_queue_front(const PedalTransferQueue *queue);

/**
 * @brief Marks the front request as active.
 *
 * An active request is withheld from subsequent front lookups until completion.
 *
 * @param[in,out] queue Queue whose front request was sent.
 */
void pedal_transfer_queue_start(PedalTransferQueue *queue);

/**
 * @brief Removes the active front request.
 *
 * Clears its length, advances the read index, and makes the next request available.
 *
 * @param[in,out] queue Queue whose active request completed.
 */
void pedal_transfer_queue_finish(PedalTransferQueue *queue);

/**
 * @brief Reports whether the front request is active.
 *
 * Checks whether a front request is currently awaiting completion.
 *
 * @param[in] queue Queue to inspect.
 * @return True while a front request awaits completion.
 */
bool pedal_transfer_queue_active(const PedalTransferQueue *queue);

#endif
