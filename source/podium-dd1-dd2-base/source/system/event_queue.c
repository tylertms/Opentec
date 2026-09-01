#include "system/event_queue.h"

#include <stdbool.h>
#include <stdint.h>

void system_event_queue_init(SystemEventQueue *queue) { *queue = (SystemEventQueue){0}; }

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

void system_event_queue_complete(SystemEventQueue *queue) {
    if (queue->count != 0) {
        queue->head = (uint8_t)((queue->head + 1) % SYSTEM_EVENT_QUEUE_CAPACITY);
        queue->count--;
    }
    queue->pending_code = queue->count == 0 ? 0 : queue->codes[queue->head];
}
