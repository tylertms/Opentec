#include "system/event_queue.h"

#include <stdbool.h>
#include <stdint.h>

void system_event_queue_init(SystemEventQueue *queue) { *queue = (SystemEventQueue){0}; }

bool system_event_queue_try_push(SystemEventQueue *queue, uint8_t code) {
    if (code == 0 || queue->pending_code != 0) {
        return false;
    }
    queue->codes[0] = code;
    queue->count = 1;
    queue->pending_code = code;
    queue->last_code = code;
    return true;
}

void system_event_queue_complete(SystemEventQueue *queue) {
    queue->count = 0;
    queue->pending_code = 0;
}
