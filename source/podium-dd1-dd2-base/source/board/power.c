#include "board/power.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    POWER_BUTTON_HOLD_MS = 1000,
    POWER_SHUTDOWN_DELAY_MS = 1000,
};

/**
 * @brief Tests whether a strict millisecond deadline has elapsed.
 *
 * Uses signed modular subtraction so both button intervals remain correct when the monotonic
 * counter wraps.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to compare against.
 * @return True only after the deadline, not at the deadline itself.
 */
static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * @brief Initializes the wheel-base power controller.
 *
 * Starts with the power hold disabled and waits for an active button sample before accepting
 * normal short-press and shutdown behavior.
 *
 * @param[out] controller Power state to initialize.
 */
void power_controller_init(PowerController *controller) { *controller = (PowerController){0}; }

/**
 * @brief Advances power-button and shutdown behavior.
 *
 * Enables the power hold on the first active sample. Subsequent short releases toggle the
 * requested on/off state. A continuously active sample starts shutdown after 1,000 ms and
 * finishes the shutdown interval after another 1,000 ms. Release is evaluated before the hold
 * deadline, including when service resumes after that deadline.
 *
 * @param[in,out] controller Persistent power phase, deadline, and requested on/off state.
 * @param[in] button_pressed True while the active-high power button input is asserted.
 * @param[in] button_control_enabled True to accept short-press and hold behavior.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The single transition action produced by this update.
 */
PowerAction power_controller_update(PowerController *controller, bool button_pressed,
                                    bool button_control_enabled, uint32_t now_ms) {
    switch (controller->phase) {
    case POWER_PHASE_WAITING_FOR_START:
        if (button_pressed) {
            controller->phase = POWER_PHASE_READY;
            return POWER_ACTION_ENABLE_LATCH;
        }
        break;
    case POWER_PHASE_READY:
        if (button_control_enabled && button_pressed) {
            controller->deadline_ms = now_ms + POWER_BUTTON_HOLD_MS;
            controller->phase = POWER_PHASE_BUTTON_HELD;
        }
        break;
    case POWER_PHASE_BUTTON_HELD:
        if (!button_pressed) {
            controller->requested_on = !controller->requested_on;
            controller->phase = POWER_PHASE_READY;
            return POWER_ACTION_REQUEST_CHANGED;
        }
        if (deadline_passed(now_ms, controller->deadline_ms)) {
            controller->requested_on = false;
            controller->deadline_ms = now_ms + POWER_SHUTDOWN_DELAY_MS;
            controller->phase = POWER_PHASE_SHUTDOWN_DELAY;
            return POWER_ACTION_BEGIN_SHUTDOWN;
        }
        break;
    case POWER_PHASE_SHUTDOWN_DELAY:
        if (deadline_passed(now_ms, controller->deadline_ms)) {
            controller->phase = POWER_PHASE_OFF;
            return POWER_ACTION_FINISH_SHUTDOWN;
        }
        break;
    case POWER_PHASE_OFF:
        break;
    }
    return POWER_ACTION_NONE;
}
