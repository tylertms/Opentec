#include "board/power.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Power-button timing constants.
 *
 * These intervals define the strict hold threshold and the delay between shutdown actions.
 */
enum {
    POWER_BUTTON_HOLD_MS = 1000, /**< Strict hold deadline interval in milliseconds. */
    POWER_SHUTDOWN_DELAY_MS =
        1000, /**< Strict shutdown-completion deadline interval in milliseconds. */
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
 * Clears the state and waits for a pressed-button sample before requesting the hardware power
 * latch; later samples then select short-press or shutdown behavior.
 *
 * @param[out] controller Power state to initialize.
 */
void power_controller_init(PowerController *controller) { *controller = (PowerController){0}; }

/**
 * @brief Advances power-button and shutdown behavior.
 *
 * Enables the power hold on the first active sample. When button control is enabled, a press while
 * ready starts the hold deadline; release is handled before checking that strict deadline and
 * toggles the torque-disable request, while a continuously active input starts shutdown after the
 * deadline and finishes after a second strict interval.
 *
 * @param[in,out] controller Persistent power phase, deadline, and torque disable request.
 * @param[in] button_pressed True while the active-high power button input is asserted.
 * @param[in] button_control_enabled True to allow a ready-state press to begin short-press or hold
 * handling.
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
            controller->torque_disabled = !controller->torque_disabled;
            controller->phase = POWER_PHASE_READY;
            return POWER_ACTION_TORQUE_REQUEST_CHANGED;
        }
        if (deadline_passed(now_ms, controller->deadline_ms)) {
            controller->torque_disabled = false;
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
