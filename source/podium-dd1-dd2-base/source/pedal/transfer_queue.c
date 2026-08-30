#include "pedal/transfer_queue.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Advances a pedal transfer queue index.
 *
 * Wraps the final usable slot back to the first slot.
 *
 * @param[in] index Current queue index.
 * @return Following index with wrap at the queue capacity.
 */
static uint8_t next_index(uint8_t index) {
    return (uint8_t)((index + 1u) % PEDAL_TRANSFER_QUEUE_CAPACITY);
}

/**
 * @brief Initializes an empty pedal transfer queue.
 *
 * Clears all retained requests and leaves no request active.
 *
 * @param[out] queue Queue to initialize.
 */
void pedal_transfer_queue_init(PedalTransferQueue *queue) { *queue = (PedalTransferQueue){0}; }

/**
 * @brief Appends one complete host request to the pedal transfer queue.
 *
 * Accepts payloads from one through 124 bytes until all eleven usable slots are occupied.
 *
 * @param[in,out] queue Queue receiving the request.
 * @param[in] data Complete logical request payload.
 * @param[in] length Request payload length.
 * @return True when the request is retained.
 */
bool pedal_transfer_queue_push(PedalTransferQueue *queue, const uint8_t *data, uint8_t length) {
    if (queue == NULL || data == NULL || length == 0 || length > PEDAL_TRANSFER_PAYLOAD_CAPACITY ||
        queue->count == PEDAL_TRANSFER_QUEUE_CAPACITY) {
        return false;
    }
    PedalTransferRequest *request = &queue->requests[queue->write_index];
    memcpy(request->data, data, length);
    request->length = length;
    queue->write_index = next_index(queue->write_index);
    queue->count++;
    return true;
}

/**
 * @brief Provides the next pedal request that can be submitted.
 *
 * Hides the front request while it is already awaiting a response.
 *
 * @param[in] queue Queue to inspect.
 * @return Next request, or null when the queue is empty or its front request is active.
 */
const PedalTransferRequest *pedal_transfer_queue_front(const PedalTransferQueue *queue) {
    return queue != NULL && queue->count != 0 && !queue->active
               ? &queue->requests[queue->read_index]
               : NULL;
}

/**
 * @brief Marks the front pedal request as awaiting a response.
 *
 * Prevents the request from being submitted again until it is finished.
 *
 * @param[in,out] queue Queue whose front request was submitted.
 */
void pedal_transfer_queue_start(PedalTransferQueue *queue) {
    if (queue != NULL && queue->count != 0) {
        queue->active = true;
    }
}

/**
 * @brief Completes the active front pedal request.
 *
 * Removes the request after either its forwarded response or its 100-millisecond response timeout.
 *
 * @param[in,out] queue Queue whose active request completed.
 */
void pedal_transfer_queue_finish(PedalTransferQueue *queue) {
    if (queue == NULL || !queue->active || queue->count == 0) {
        return;
    }
    queue->requests[queue->read_index].length = 0;
    queue->read_index = next_index(queue->read_index);
    queue->count--;
    queue->active = false;
}

/**
 * @brief Reports whether the front pedal request is awaiting completion.
 *
 * Distinguishes a submitted front request from queued work that is ready to start.
 *
 * @param[in] queue Queue to inspect.
 * @return True while a submitted request remains at the front.
 */
bool pedal_transfer_queue_active(const PedalTransferQueue *queue) {
    return queue != NULL && queue->active;
}
