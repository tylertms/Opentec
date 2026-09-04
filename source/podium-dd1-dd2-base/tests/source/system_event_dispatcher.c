#include <assert.h>
#include <stddef.h>
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

static void test_dispatches_motor_notice_actions(void) {
    static const struct {
        uint8_t code;
        SystemEventAction action;
    } cases[] = {
        {1, SYSTEM_EVENT_ACTION_SHOW_TUNING_MENU_RESET},
        {2, SYSTEM_EVENT_ACTION_SHOW_WHEEL_CENTER_CALIBRATED},
        {3, SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_SUCCEEDED},
        {4, SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_STARTED},
        {5, SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_FAILED},
        {6, SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED},
        {0x16, SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED_STEERING_WHEEL},
        {7, SYSTEM_EVENT_ACTION_SHOW_TORQUE_KEY_PROMPT},
        {8, SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_DISCONNECT_WHEEL},
        {9, SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_UNSUPPORTED},
        {10, SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_COMPLETED},
        {11, SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ERASED},
        {12, SYSTEM_EVENT_ACTION_SHOW_FORCE_OUTPUT_PROMPT},
        {0x0e, SYSTEM_EVENT_ACTION_SHOW_SHUTDOWN},
        {0x0f, SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_INVERTED},
        {0x10, SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_OUTLINED},
        {0x11, SYSTEM_EVENT_ACTION_DISMISS_CURRENT_NOTICE},
        {0x12, SYSTEM_EVENT_ACTION_SHOW_STANDARD_TUNING_MODE},
        {0x13, SYSTEM_EVENT_ACTION_SHOW_ADVANCED_TUNING_MODE},
        {0x14, SYSTEM_EVENT_ACTION_SHOW_TUNING_MODE_TRANSITION_STANDARD},
        {0x15, SYSTEM_EVENT_ACTION_SHOW_TUNING_MODE_TRANSITION_ADVANCED},
        {0x17, SYSTEM_EVENT_ACTION_SHOW_MAXIMUM_ROTATIONS_EXCEEDED},
        {0x18, SYSTEM_EVENT_ACTION_DISMISS_TORQUE_KEY_PROMPT},
        {0x19, SYSTEM_EVENT_ACTION_DISMISS_TORQUE_REDUCED},
        {0x1a, SYSTEM_EVENT_ACTION_DISMISS_FORCE_OUTPUT_PROMPT},
        {0x1c, SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ONGOING},
        {0x1d, SYSTEM_EVENT_ACTION_DISMISS_MOTOR_CALIBRATION},
        {0x20, SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_ENABLED},
        {0x21, SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_DISABLED},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        SystemEventDispatcher dispatcher;
        SystemEventQueue queue;
        system_event_dispatcher_init(&dispatcher);
        system_event_queue_init(&queue);
        assert(system_event_queue_try_push(&queue, cases[index].code));

        assert(system_event_dispatcher_update(&dispatcher, &queue, 0) == cases[index].action);
        assert(queue.pending_code == 0);
        assert(queue.last_code == cases[index].code);
    }
}

static void test_releases_unknown_events(void) {
    SystemEventDispatcher dispatcher;
    SystemEventQueue queue;
    system_event_dispatcher_init(&dispatcher);
    system_event_queue_init(&queue);
    assert(system_event_queue_try_push(&queue, 0x7f));

    assert(system_event_dispatcher_update(&dispatcher, &queue, 1000) == SYSTEM_EVENT_ACTION_NONE);
    assert(queue.pending_code == 0);
    assert(queue.count == 0);
    assert(queue.last_code == 0x7f);
    assert(dispatcher.next_dispatch_ms == 0);

    assert(system_event_queue_try_push(&queue, 0x11));
    assert(system_event_dispatcher_update(&dispatcher, &queue, 1000) ==
           SYSTEM_EVENT_ACTION_DISMISS_CURRENT_NOTICE);
    assert(queue.pending_code == 0);
    assert(queue.last_code == 0x11);
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
    test_dispatches_motor_notice_actions();
    test_releases_unknown_events();
    test_preserves_cadence_across_counter_wrap();
    return 0;
}
