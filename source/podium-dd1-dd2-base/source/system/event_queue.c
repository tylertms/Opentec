#include "system/event_queue.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the single-slot system event queue.
 *
 * Clears both the pending event and the retained last event code.
 *
 * @param[out] queue Event queue to initialize.
 */
void system_event_queue_init(SystemEventQueue *queue) { *queue = (SystemEventQueue){0}; }

/**
 * @brief Attempts to queue one system event code.
 *
 * Accepts a nonzero code only while no event is pending and retains the accepted code as both the
 * pending and last event. A busy queue remains unchanged.
 *
 * @param[in,out] queue Single-slot event queue.
 * @param[in] code Nonzero system event code to queue.
 * @return True when the code was accepted.
 */
bool system_event_queue_try_push(SystemEventQueue *queue, uint8_t code) {
    if (code == 0 || queue->count == SYSTEM_EVENT_QUEUE_CAPACITY) {
        return false;
    }
    uint8_t index = (uint8_t)((queue->head + queue->count) % SYSTEM_EVENT_QUEUE_CAPACITY);
    queue->codes[index] = code;
    queue->count++;
    queue->pending_code = queue->codes[queue->head];
    queue->last_code = code;
    return true;
}

/**
 * @brief Completes the pending system event.
 *
 * Releases the queue slot while preserving the last accepted event code.
 *
 * @param[in,out] queue Single-slot event queue.
 */
void system_event_queue_complete(SystemEventQueue *queue) {
    if (queue->count != 0) {
        queue->head = (uint8_t)((queue->head + 1) % SYSTEM_EVENT_QUEUE_CAPACITY);
        queue->count--;
    }
    queue->pending_code = queue->count == 0 ? 0 : queue->codes[queue->head];
}
