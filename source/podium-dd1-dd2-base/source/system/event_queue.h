#ifndef OPENTEC_BASE_SYSTEM_EVENT_QUEUE_H
#define OPENTEC_BASE_SYSTEM_EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t pending_code;
    uint8_t last_code;
} SystemEventQueue;

void system_event_queue_init(SystemEventQueue *queue);
bool system_event_queue_try_push(SystemEventQueue *queue, uint8_t code);
void system_event_queue_complete(SystemEventQueue *queue);

#endif
