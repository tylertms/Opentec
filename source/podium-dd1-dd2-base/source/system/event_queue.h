#ifndef OPENTEC_BASE_SYSTEM_EVENT_QUEUE_H
#define OPENTEC_BASE_SYSTEM_EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

enum { SYSTEM_EVENT_QUEUE_CAPACITY = 5 };

typedef struct {
    uint8_t codes[SYSTEM_EVENT_QUEUE_CAPACITY];
    uint8_t pending_code;
    uint8_t last_code;
    uint8_t head;
    uint8_t count;
} SystemEventQueue;

void system_event_queue_init(SystemEventQueue *queue);
bool system_event_queue_try_push(SystemEventQueue *queue, uint8_t code);
void system_event_queue_complete(SystemEventQueue *queue);

#endif
