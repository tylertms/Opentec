#include "system/event_dispatcher.h"

#include <stdbool.h>
#include <stdint.h>

#include "system/event_queue.h"

enum {
    SYSTEM_EVENT_TORQUE_DISABLED = 0x0d,
    SYSTEM_EVENT_TORQUE_ENABLED = 0x1b,
    SYSTEM_EVENT_DISPATCH_INTERVAL_MS = 100,
};

/**
 * @brief Tests whether the event dispatch cadence permits another action.
 *
 * Uses signed modular subtraction so the cadence remains valid across the millisecond counter
 * wrap.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] next_dispatch_ms Earliest permitted dispatch time.
 * @return True at or after the permitted dispatch time.
 */
static bool dispatch_due(uint32_t now_ms, uint32_t next_dispatch_ms) {
    return (int32_t)(now_ms - next_dispatch_ms) >= 0;
}

/**
 * @brief Initializes system event dispatch timing.
 *
 * Allows the first recognized event to dispatch immediately.
 *
 * @param[out] dispatcher Event dispatcher to initialize.
 */
void system_event_dispatcher_init(SystemEventDispatcher *dispatcher) {
    *dispatcher = (SystemEventDispatcher){0};
}

/**
 * @brief Dispatches a queued power-button torque event.
 *
 * Event 0x0d requests the torque-disabled notice and event 0x1b dismisses it. Recognized events
 * complete their queue slot and start a 100-millisecond minimum interval. Other event codes remain
 * available for their owning dispatcher.
 *
 * @param[in,out] dispatcher Shared event dispatch cadence.
 * @param[in,out] queue Single-slot system event queue.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Semantic action for the notice owner, or no action while idle or waiting.
 */
SystemEventAction system_event_dispatcher_update(SystemEventDispatcher *dispatcher,
                                                 SystemEventQueue *queue, uint32_t now_ms) {
    SystemEventAction action;
    if (queue->pending_code == SYSTEM_EVENT_TORQUE_DISABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_ENABLED) {
        action = SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED;
    } else {
        return SYSTEM_EVENT_ACTION_NONE;
    }
    if (!dispatch_due(now_ms, dispatcher->next_dispatch_ms)) {
        return SYSTEM_EVENT_ACTION_NONE;
    }

    system_event_queue_complete(queue);
    dispatcher->next_dispatch_ms = now_ms + SYSTEM_EVENT_DISPATCH_INTERVAL_MS;
    return action;
}
