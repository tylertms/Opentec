#ifndef OPENTEC_BASE_SYSTEM_EVENT_DISPATCHER_H
#define OPENTEC_BASE_SYSTEM_EVENT_DISPATCHER_H

#include <stdint.h>

#include "system/event_queue.h"

/**
 * @brief Presentation or dismissal action emitted for a recognized system event.
 *
 * Each value identifies the display or prompt operation that the firmware integration layer must
 * perform after consuming the corresponding event code.
 */
typedef enum {
    SYSTEM_EVENT_ACTION_NONE, /**< No recognized event is ready for presentation. */
    SYSTEM_EVENT_ACTION_SHOW_TUNING_MENU_RESET,       /**< Show the tuning-menu reset notice. */
    SYSTEM_EVENT_ACTION_SHOW_WHEEL_CENTER_CALIBRATED, /**< Show the wheel-center notice. */
    SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_SUCCEEDED, /**< Show a successful sensor test. */
    SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_STARTED,   /**< Show a sensor test in progress. */
    SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_FAILED,    /**< Show a failed sensor test. */
    SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED,                 /**< Show reduced torque. */
    SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED_STEERING_WHEEL, /**< Show reduced steering-wheel torque.
                                                             */
    SYSTEM_EVENT_ACTION_SHOW_TORQUE_KEY_PROMPT,             /**< Show the Torque Key prompt. */
    SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_DISCONNECT_WHEEL, /**< Request wheel disconnection.
                                                                  */
    SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_UNSUPPORTED, /**< Show unsupported calibration. */
    SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ONGOING,     /**< Show calibration in progress. */
    SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_COMPLETED,   /**< Show completed calibration. */
    SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ERASED,      /**< Show erased calibration. */
    SYSTEM_EVENT_ACTION_SHOW_FORCE_OUTPUT_PROMPT,           /**< Show the force-output prompt. */
    SYSTEM_EVENT_ACTION_DISMISS_FORCE_OUTPUT_PROMPT,        /**< Dismiss the force-output prompt. */
    SYSTEM_EVENT_ACTION_SHOW_STANDARD_TUNING_MODE,          /**< Show standard tuning mode. */
    SYSTEM_EVENT_ACTION_SHOW_ADVANCED_TUNING_MODE,          /**< Show advanced tuning mode. */
    SYSTEM_EVENT_ACTION_SHOW_MAXIMUM_ROTATIONS_EXCEEDED,    /**< Show the maximum-rotation alert. */
    SYSTEM_EVENT_ACTION_SHOW_SHUTDOWN,                      /**< Show the shutdown notice. */
    SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_INVERTED,    /**< Show an inverted-wheel alert. */
    SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_OUTLINED,    /**< Show an outlined-wheel alert. */
    SYSTEM_EVENT_ACTION_DISMISS_TORQUE_KEY_PROMPT,          /**< Dismiss the Torque Key prompt. */
    SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED,               /**< Show the torque-disabled notice. */
    SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED, /**< Dismiss the torque-disabled notice. */
    SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_ENABLED,  /**< Show alternative-shifter enabled. */
    SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_DISABLED, /**< Show alternative-shifter disabled.
                                                            */
} SystemEventAction;

/**
 * @brief Event dispatch cadence state.
 *
 * The dispatcher uses the deadline to enforce the minimum interval between recognized event
 * actions.
 */
typedef struct {
    uint32_t next_dispatch_ms; /**< Earliest time at which another event may be dispatched. */
} SystemEventDispatcher;

/**
 * @brief Initializes system event dispatch timing.
 *
 * Clears the next-dispatch deadline so the first recognized event can dispatch immediately.
 *
 * @param[out] dispatcher Event dispatcher to initialize.
 */
void system_event_dispatcher_init(SystemEventDispatcher *dispatcher);

/**
 * @brief Dispatches the oldest recognized system event.
 *
 * Maps recognized event codes to presentation actions, consumes the queue entry, and starts the
 * minimum dispatch interval when the cadence permits an action.
 *
 * @param[in,out] dispatcher Event dispatcher cadence state.
 * @param[in,out] queue System event queue containing the pending code.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Presentation action, or SYSTEM_EVENT_ACTION_NONE while idle or waiting.
 */
SystemEventAction system_event_dispatcher_update(SystemEventDispatcher *dispatcher,
                                                 SystemEventQueue *queue, uint32_t now_ms);

#endif
