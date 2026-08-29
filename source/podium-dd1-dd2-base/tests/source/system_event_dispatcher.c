#include <assert.h>
#include <stdint.h>

#include "system/event_dispatcher.h"
#include "system/event_queue.h"

static void test_dispatches_torque_notice_actions(void) {
    SystemEventDispatcher dispatcher;
    SystemEventQueue queue;
    system_event_dispatcher_init(&dispatcher);
    system_event_queue_init(&queue);

    assert(system_event_dispatcher_update(&dispatcher, &queue, 0) == SYSTEM_EVENT_ACTION_NONE);
    assert(system_event_queue_try_push(&queue, 0x0d));
    assert(system_event_dispatcher_update(&dispatcher, &queue, 0) ==
           SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED);
    assert(queue.pending_code == 0);
    assert(queue.last_code == 0x0d);

    assert(system_event_queue_try_push(&queue, 0x1b));
    assert(system_event_dispatcher_update(&dispatcher, &queue, 99) == SYSTEM_EVENT_ACTION_NONE);
    assert(queue.pending_code == 0x1b);
    assert(system_event_dispatcher_update(&dispatcher, &queue, 100) ==
           SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED);
    assert(queue.pending_code == 0);
    assert(queue.last_code == 0x1b);
}

static void test_leaves_unowned_events_pending(void) {
    SystemEventDispatcher dispatcher;
    SystemEventQueue queue;
    system_event_dispatcher_init(&dispatcher);
    system_event_queue_init(&queue);
    assert(system_event_queue_try_push(&queue, 0x0e));

    assert(system_event_dispatcher_update(&dispatcher, &queue, 1000) == SYSTEM_EVENT_ACTION_NONE);
    assert(queue.pending_code == 0x0e);
    assert(dispatcher.next_dispatch_ms == 0);
}

static void test_preserves_cadence_across_counter_wrap(void) {
    SystemEventDispatcher dispatcher = {.next_dispatch_ms = UINT32_MAX - 50};
    SystemEventQueue queue;
    system_event_queue_init(&queue);
    assert(system_event_queue_try_push(&queue, 0x0d));

    assert(system_event_dispatcher_update(&dispatcher, &queue, UINT32_MAX - 50) ==
           SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED);
    assert(dispatcher.next_dispatch_ms == 49);
    assert(system_event_queue_try_push(&queue, 0x1b));
    assert(system_event_dispatcher_update(&dispatcher, &queue, 48) == SYSTEM_EVENT_ACTION_NONE);
    assert(system_event_dispatcher_update(&dispatcher, &queue, 49) ==
           SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED);
    assert(dispatcher.next_dispatch_ms == 149);
}

int main(void) {
    test_dispatches_torque_notice_actions();
    test_leaves_unowned_events_pending();
    test_preserves_cadence_across_counter_wrap();
    return 0;
}
