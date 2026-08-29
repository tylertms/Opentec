#ifndef OPENTEC_BASE_SYSTEM_EVENT_DISPATCHER_H
#define OPENTEC_BASE_SYSTEM_EVENT_DISPATCHER_H

#include <stdint.h>

#include "system/event_queue.h"

typedef enum {
    SYSTEM_EVENT_ACTION_NONE,
    SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED,
    SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED,
} SystemEventAction;

typedef struct {
    uint32_t next_dispatch_ms;
} SystemEventDispatcher;

void system_event_dispatcher_init(SystemEventDispatcher *dispatcher);
SystemEventAction system_event_dispatcher_update(SystemEventDispatcher *dispatcher,
                                                 SystemEventQueue *queue, uint32_t now_ms);

#endif
