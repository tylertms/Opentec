#include "system/event_dispatcher.h"

#include <stdbool.h>
#include <stdint.h>

#include "system/event_queue.h"

enum {
    SYSTEM_EVENT_TUNING_MENU_RESET = 1,
    SYSTEM_EVENT_WHEEL_CENTER_CALIBRATED = 2,
    SYSTEM_EVENT_POSITION_SENSOR_TEST_SUCCEEDED = 3,
    SYSTEM_EVENT_POSITION_SENSOR_TEST_STARTED = 4,
    SYSTEM_EVENT_POSITION_SENSOR_TEST_FAILED = 5,
    SYSTEM_EVENT_TORQUE_REDUCED = 6,
    SYSTEM_EVENT_TORQUE_KEY_PROMPT = 7,
    SYSTEM_EVENT_MOTOR_CALIBRATION_DISCONNECT_WHEEL = 8,
    SYSTEM_EVENT_MOTOR_CALIBRATION_UNSUPPORTED = 9,
    SYSTEM_EVENT_MOTOR_CALIBRATION_COMPLETED = 10,
    SYSTEM_EVENT_MOTOR_CALIBRATION_ERASED = 11,
    SYSTEM_EVENT_FORCE_OUTPUT_PROMPT = 12,
    SYSTEM_EVENT_TORQUE_DISABLED = 0x0d,
    SYSTEM_EVENT_SHUTDOWN = 0x0e,
    SYSTEM_EVENT_UNSUPPORTED_WHEEL_INVERTED = 0x0f,
    SYSTEM_EVENT_UNSUPPORTED_WHEEL_OUTLINED = 0x10,
    SYSTEM_EVENT_STANDARD_TUNING_MODE = 0x12,
    SYSTEM_EVENT_ADVANCED_TUNING_MODE = 0x13,
    SYSTEM_EVENT_MAXIMUM_ROTATIONS_EXCEEDED = 0x17,
    SYSTEM_EVENT_DISMISS_TORQUE_KEY_PROMPT = 0x18,
    SYSTEM_EVENT_DISMISS_FORCE_OUTPUT_PROMPT = 0x1a,
    SYSTEM_EVENT_TORQUE_ENABLED = 0x1b,
    SYSTEM_EVENT_MOTOR_CALIBRATION_ONGOING = 0x1c,
    SYSTEM_EVENT_ALTERNATIVE_SHIFTER_ENABLED = 0x20,
    SYSTEM_EVENT_ALTERNATIVE_SHIFTER_DISABLED = 0x21,
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
 * @brief Dispatches a queued system notice event.
 *
 * Maps tuning-menu, wheel-center, position-sensor, motor-calibration, Torque Key,
 * unsupported-wheel, force-output, torque-reduction, and power-button event codes to display
 * actions. Recognized events complete their queue slot and start a 100-millisecond minimum
 * interval. Other event codes remain available for their owning dispatcher.
 *
 * @param[in,out] dispatcher Shared event dispatch cadence.
 * @param[in,out] queue Single-slot system event queue.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Semantic action for the notice owner, or no action while idle or waiting.
 */
SystemEventAction system_event_dispatcher_update(SystemEventDispatcher *dispatcher,
                                                 SystemEventQueue *queue, uint32_t now_ms) {
    SystemEventAction action;
    if (queue->pending_code == SYSTEM_EVENT_TUNING_MENU_RESET) {
        action = SYSTEM_EVENT_ACTION_SHOW_TUNING_MENU_RESET;
    } else if (queue->pending_code == SYSTEM_EVENT_WHEEL_CENTER_CALIBRATED) {
        action = SYSTEM_EVENT_ACTION_SHOW_WHEEL_CENTER_CALIBRATED;
    } else if (queue->pending_code == SYSTEM_EVENT_POSITION_SENSOR_TEST_SUCCEEDED) {
        action = SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_SUCCEEDED;
    } else if (queue->pending_code == SYSTEM_EVENT_POSITION_SENSOR_TEST_STARTED) {
        action = SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_STARTED;
    } else if (queue->pending_code == SYSTEM_EVENT_POSITION_SENSOR_TEST_FAILED) {
        action = SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_FAILED;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_REDUCED) {
        action = SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_KEY_PROMPT) {
        action = SYSTEM_EVENT_ACTION_SHOW_TORQUE_KEY_PROMPT;
    } else if (queue->pending_code == SYSTEM_EVENT_MOTOR_CALIBRATION_DISCONNECT_WHEEL) {
        action = SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_DISCONNECT_WHEEL;
    } else if (queue->pending_code == SYSTEM_EVENT_MOTOR_CALIBRATION_UNSUPPORTED) {
        action = SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_UNSUPPORTED;
    } else if (queue->pending_code == SYSTEM_EVENT_MOTOR_CALIBRATION_ONGOING) {
        action = SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ONGOING;
    } else if (queue->pending_code == SYSTEM_EVENT_MOTOR_CALIBRATION_COMPLETED) {
        action = SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_COMPLETED;
    } else if (queue->pending_code == SYSTEM_EVENT_MOTOR_CALIBRATION_ERASED) {
        action = SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ERASED;
    } else if (queue->pending_code == SYSTEM_EVENT_FORCE_OUTPUT_PROMPT) {
        action = SYSTEM_EVENT_ACTION_SHOW_FORCE_OUTPUT_PROMPT;
    } else if (queue->pending_code == SYSTEM_EVENT_DISMISS_FORCE_OUTPUT_PROMPT) {
        action = SYSTEM_EVENT_ACTION_DISMISS_FORCE_OUTPUT_PROMPT;
    } else if (queue->pending_code == SYSTEM_EVENT_STANDARD_TUNING_MODE) {
        action = SYSTEM_EVENT_ACTION_SHOW_STANDARD_TUNING_MODE;
    } else if (queue->pending_code == SYSTEM_EVENT_ADVANCED_TUNING_MODE) {
        action = SYSTEM_EVENT_ACTION_SHOW_ADVANCED_TUNING_MODE;
    } else if (queue->pending_code == SYSTEM_EVENT_MAXIMUM_ROTATIONS_EXCEEDED) {
        action = SYSTEM_EVENT_ACTION_SHOW_MAXIMUM_ROTATIONS_EXCEEDED;
    } else if (queue->pending_code == SYSTEM_EVENT_SHUTDOWN) {
        action = SYSTEM_EVENT_ACTION_SHOW_SHUTDOWN;
    } else if (queue->pending_code == SYSTEM_EVENT_UNSUPPORTED_WHEEL_INVERTED) {
        action = SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_INVERTED;
    } else if (queue->pending_code == SYSTEM_EVENT_UNSUPPORTED_WHEEL_OUTLINED) {
        action = SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_OUTLINED;
    } else if (queue->pending_code == SYSTEM_EVENT_DISMISS_TORQUE_KEY_PROMPT) {
        action = SYSTEM_EVENT_ACTION_DISMISS_TORQUE_KEY_PROMPT;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_DISABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_ENABLED) {
        action = SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_ALTERNATIVE_SHIFTER_ENABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_ENABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_ALTERNATIVE_SHIFTER_DISABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_DISABLED;
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
