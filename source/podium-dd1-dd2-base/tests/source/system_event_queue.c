#include <assert.h>

#include "system/event_queue.h"

static void test_retains_one_nonzero_event(void) {
    SystemEventQueue queue;
    system_event_queue_init(&queue);

    assert(queue.pending_code == 0);
    assert(queue.last_code == 0);
    assert(!system_event_queue_try_push(&queue, 0));
    assert(system_event_queue_try_push(&queue, 0x0d));
    assert(queue.pending_code == 0x0d);
    assert(queue.last_code == 0x0d);
    assert(!system_event_queue_try_push(&queue, 0x1b));
    assert(!system_event_queue_try_push(&queue, 0x2c));
    assert(!system_event_queue_try_push(&queue, 0x3d));
    assert(!system_event_queue_try_push(&queue, 0x4e));
    assert(!system_event_queue_try_push(&queue, 0x5f));
    assert(queue.pending_code == 0x0d);
    assert(queue.last_code == 0x0d);
}

static void test_completion_preserves_last_event(void) {
    SystemEventQueue queue;
    system_event_queue_init(&queue);
    assert(system_event_queue_try_push(&queue, 0x0d));

    system_event_queue_complete(&queue);
    assert(queue.pending_code == 0);
    assert(queue.last_code == 0x0d);
    assert(system_event_queue_try_push(&queue, 0x1b));
    assert(queue.pending_code == 0x1b);
    assert(queue.last_code == 0x1b);
}

int main(void) {
    test_retains_one_nonzero_event();
    test_completion_preserves_last_event();
    return 0;
}
