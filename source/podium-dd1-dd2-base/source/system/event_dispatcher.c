#include "system/event_dispatcher.h"

#include <stdbool.h>
#include <stdint.h>

#include "system/event_queue.h"

/**
 * @brief Internal system event codes and dispatch interval.
 *
 * The dispatcher maps these wire event codes to public presentation actions.
 */
enum {
    SYSTEM_EVENT_TUNING_MENU_RESET = 1,                  /**< Tuning-menu reset event code. */
    SYSTEM_EVENT_WHEEL_CENTER_CALIBRATED = 2,            /**< Wheel-center calibrated event code. */
    SYSTEM_EVENT_POSITION_SENSOR_TEST_SUCCEEDED = 3,     /**< Successful sensor-test event code. */
    SYSTEM_EVENT_POSITION_SENSOR_TEST_STARTED = 4,       /**< Sensor-test started event code. */
    SYSTEM_EVENT_POSITION_SENSOR_TEST_FAILED = 5,        /**< Failed sensor-test event code. */
    SYSTEM_EVENT_TORQUE_REDUCED = 6,                     /**< Reduced-torque event code. */
    SYSTEM_EVENT_TORQUE_REDUCED_STEERING_WHEEL = 0x16,   /**< Reduced steering-wheel torque code. */
    SYSTEM_EVENT_TORQUE_KEY_PROMPT = 7,                  /**< Torque Key prompt event code. */
    SYSTEM_EVENT_MOTOR_CALIBRATION_DISCONNECT_WHEEL = 8, /**< Calibration disconnect event code. */
    SYSTEM_EVENT_MOTOR_CALIBRATION_UNSUPPORTED = 9,      /**< Unsupported calibration event code. */
    SYSTEM_EVENT_MOTOR_CALIBRATION_COMPLETED = 10,       /**< Calibration completed event code. */
    SYSTEM_EVENT_MOTOR_CALIBRATION_ERASED = 11,          /**< Calibration erased event code. */
    SYSTEM_EVENT_FORCE_OUTPUT_PROMPT = 12,               /**< Force-output prompt event code. */
    SYSTEM_EVENT_TORQUE_DISABLED = 0x0d,                 /**< Torque-disabled event code. */
    SYSTEM_EVENT_SHUTDOWN = 0x0e,                        /**< Shutdown event code. */
    SYSTEM_EVENT_UNSUPPORTED_WHEEL_INVERTED = 0x0f,      /**< Inverted-wheel event code. */
    SYSTEM_EVENT_UNSUPPORTED_WHEEL_OUTLINED = 0x10,      /**< Outlined-wheel event code. */
    SYSTEM_EVENT_DISMISS_CURRENT_NOTICE = 0x11,          /**< Active-notice dismissal code. */
    SYSTEM_EVENT_STANDARD_TUNING_MODE = 0x12,            /**< Standard tuning-mode event code. */
    SYSTEM_EVENT_ADVANCED_TUNING_MODE = 0x13,            /**< Advanced tuning-mode event code. */
    SYSTEM_EVENT_TUNING_MODE_TRANSITION_STANDARD = 0x14, /**< Standard-mode transition code. */
    SYSTEM_EVENT_TUNING_MODE_TRANSITION_ADVANCED = 0x15, /**< Advanced-mode transition code. */
    SYSTEM_EVENT_MAXIMUM_ROTATIONS_EXCEEDED = 0x17,      /**< Maximum-rotation event code. */
    SYSTEM_EVENT_DISMISS_TORQUE_KEY_PROMPT = 0x18,       /**< Torque Key dismissal event code. */
    SYSTEM_EVENT_DISMISS_TORQUE_REDUCED = 0x19,          /**< Reduced-torque dismissal code. */
    SYSTEM_EVENT_DISMISS_FORCE_OUTPUT_PROMPT = 0x1a,     /**< Force-output dismissal event code. */
    SYSTEM_EVENT_TORQUE_ENABLED = 0x1b,                  /**< Torque-enabled event code. */
    SYSTEM_EVENT_MOTOR_CALIBRATION_ONGOING = 0x1c,       /**< Calibration-ongoing event code. */
    SYSTEM_EVENT_DISMISS_MOTOR_CALIBRATION = 0x1d,       /**< Calibration dismissal code. */
    SYSTEM_EVENT_ALTERNATIVE_SHIFTER_ENABLED = 0x20,     /**< Alternative-shifter enabled code. */
    SYSTEM_EVENT_ALTERNATIVE_SHIFTER_DISABLED = 0x21,    /**< Alternative-shifter disabled code. */
    SYSTEM_EVENT_DISPATCH_INTERVAL_MS = 100, /**< Minimum interval between dispatched events. */
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

void system_event_dispatcher_init(SystemEventDispatcher *dispatcher) {
    *dispatcher = (SystemEventDispatcher){0};
}

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
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_REDUCED_STEERING_WHEEL) {
        action = SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED_STEERING_WHEEL;
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
    } else if (queue->pending_code == SYSTEM_EVENT_TUNING_MODE_TRANSITION_STANDARD) {
        action = SYSTEM_EVENT_ACTION_SHOW_TUNING_MODE_TRANSITION_STANDARD;
    } else if (queue->pending_code == SYSTEM_EVENT_TUNING_MODE_TRANSITION_ADVANCED) {
        action = SYSTEM_EVENT_ACTION_SHOW_TUNING_MODE_TRANSITION_ADVANCED;
    } else if (queue->pending_code == SYSTEM_EVENT_MAXIMUM_ROTATIONS_EXCEEDED) {
        action = SYSTEM_EVENT_ACTION_SHOW_MAXIMUM_ROTATIONS_EXCEEDED;
    } else if (queue->pending_code == SYSTEM_EVENT_SHUTDOWN) {
        action = SYSTEM_EVENT_ACTION_SHOW_SHUTDOWN;
    } else if (queue->pending_code == SYSTEM_EVENT_UNSUPPORTED_WHEEL_INVERTED) {
        action = SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_INVERTED;
    } else if (queue->pending_code == SYSTEM_EVENT_UNSUPPORTED_WHEEL_OUTLINED) {
        action = SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_OUTLINED;
    } else if (queue->pending_code == SYSTEM_EVENT_DISMISS_CURRENT_NOTICE) {
        action = SYSTEM_EVENT_ACTION_DISMISS_CURRENT_NOTICE;
    } else if (queue->pending_code == SYSTEM_EVENT_DISMISS_TORQUE_KEY_PROMPT) {
        action = SYSTEM_EVENT_ACTION_DISMISS_TORQUE_KEY_PROMPT;
    } else if (queue->pending_code == SYSTEM_EVENT_DISMISS_TORQUE_REDUCED) {
        action = SYSTEM_EVENT_ACTION_DISMISS_TORQUE_REDUCED;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_DISABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_TORQUE_ENABLED) {
        action = SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_DISMISS_MOTOR_CALIBRATION) {
        action = SYSTEM_EVENT_ACTION_DISMISS_MOTOR_CALIBRATION;
    } else if (queue->pending_code == SYSTEM_EVENT_ALTERNATIVE_SHIFTER_ENABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_ENABLED;
    } else if (queue->pending_code == SYSTEM_EVENT_ALTERNATIVE_SHIFTER_DISABLED) {
        action = SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_DISABLED;
    } else {
        system_event_queue_complete(queue);
        return SYSTEM_EVENT_ACTION_NONE;
    }
    if (!dispatch_due(now_ms, dispatcher->next_dispatch_ms)) {
        return SYSTEM_EVENT_ACTION_NONE;
    }

    system_event_queue_complete(queue);
    dispatcher->next_dispatch_ms = now_ms + SYSTEM_EVENT_DISPATCH_INTERVAL_MS;
    return action;
}
