#include "board/power_button.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    POWER_BUTTON_DEBOUNCE_MS = 200,
    POWER_BUTTON_DISPLAY_BLANK_MS = 2000,
};

void power_button_init(PowerButton *button) { *button = (PowerButton){0}; }

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * Debounces the power button and emits the shutdown and display actions for a held press.
 *
 * @param button Persistent debounce, display deadline, and action-latch state.
 * @param pressed True while the active-low power-button input is asserted.
 * @param enabled True when the power-button display action is available.
 * @param now_ms Current millisecond counter.
 * @return Bitwise combination of shutdown, display blank, and display clear actions.
 */
PowerButtonAction power_button_update(PowerButton *button, bool pressed, bool enabled,
                                      uint32_t now_ms) {
    PowerButtonAction actions = POWER_BUTTON_ACTION_NONE;

    if (!pressed) {
        button->active = false;
        button->shutdown_started = false;
        button->press_ready_ms = now_ms + POWER_BUTTON_DEBOUNCE_MS;
    } else if (enabled && deadline_reached(now_ms, button->press_ready_ms)) {
        if (!button->shutdown_started) {
            actions |= POWER_BUTTON_ACTION_SHUTDOWN;
            button->clear_after_ms = now_ms + POWER_BUTTON_DISPLAY_BLANK_MS;
            button->shutdown_started = true;
        }
        if (!deadline_passed(now_ms, button->clear_after_ms)) {
            actions |= POWER_BUTTON_ACTION_BLANK_DISPLAY;
        }
        button->active = true;
    }

    if (button->clear_after_ms != 0 && deadline_passed(now_ms, button->clear_after_ms)) {
        actions |= POWER_BUTTON_ACTION_CLEAR_DISPLAY;
        button->clear_after_ms = 0;
    }

    return actions;
}
