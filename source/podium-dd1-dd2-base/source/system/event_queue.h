#ifndef OPENTEC_BASE_SYSTEM_EVENT_QUEUE_H
#define OPENTEC_BASE_SYSTEM_EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Capacity of the system event ring queue.
 *
 * The queue retains up to five pending event codes before a new push is rejected.
 */
enum {
    SYSTEM_EVENT_QUEUE_CAPACITY = 5 /**< Number of event codes retained by the ring queue. */
};

/**
 * @brief FIFO storage for pending system event codes.
 *
 * The queue retains up to SYSTEM_EVENT_QUEUE_CAPACITY codes and exposes the oldest code through
 * pending_code for dispatch.
 */
typedef struct {
    uint8_t codes[SYSTEM_EVENT_QUEUE_CAPACITY]; /**< Ring storage for queued event codes. */
    uint8_t pending_code; /**< Oldest queued event code, or zero when the queue is empty. */
    uint8_t last_code;    /**< Most recently accepted nonzero event code. */
    uint8_t head;         /**< Index of the oldest queued code in codes. */
    uint8_t count;        /**< Number of queued event codes. */
} SystemEventQueue;

/**
 * @brief Initializes the system event queue.
 *
 * Clears all queued codes, the pending code, and the retained last-code value.
 *
 * @param[out] queue Event queue to initialize.
 */
void system_event_queue_init(SystemEventQueue *queue);

/**
 * @brief Attempts to queue one system event code.
 *
 * Appends a nonzero code while capacity remains and exposes the oldest code for dispatch; a full
 * queue or zero code leaves the queue unchanged.
 *
 * @param[in,out] queue System event queue receiving the code.
 * @param[in] code Nonzero system event code to queue.
 * @return True when the code was accepted; otherwise false.
 */
bool system_event_queue_try_push(SystemEventQueue *queue, uint8_t code);

/**
 * @brief Completes the oldest queued system event.
 *
 * Removes the pending code from the ring and exposes the next queued code, while preserving the
 * most recently accepted event in last_code.
 *
 * @param[in,out] queue System event queue to advance.
 */
void system_event_queue_complete(SystemEventQueue *queue);

#endif
