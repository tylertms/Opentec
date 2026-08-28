#include "board/status_led.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    STATUS_LED_INITIAL_DELAY_MS = 2,
    STATUS_LED_ON_DURATION_MS = 1,
    STATUS_LED_END_DELAY_MS = 1,
};

void status_led_init(StatusLed *led) { *led = (StatusLed){0}; }

static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * Advances the repeating status LED pulse and returns its active-low logical state.
 *
 * @param led Persistent pulse phase, deadline, and logical output state.
 * @param now_ms Current millisecond counter.
 * @return True while the pulse is in its active phase; otherwise false.
 */
bool status_led_update(StatusLed *led, uint32_t now_ms) {
    switch (led->phase) {
    case STATUS_LED_CYCLE_START:
        led->deadline_ms = now_ms + STATUS_LED_INITIAL_DELAY_MS;
        led->phase = STATUS_LED_WAITING_TO_TURN_ON;
        break;
    case STATUS_LED_WAITING_TO_TURN_ON:
        if (deadline_passed(now_ms, led->deadline_ms)) {
            led->on = true;
            led->deadline_ms = now_ms + STATUS_LED_ON_DURATION_MS;
            led->phase = STATUS_LED_WAITING_TO_TURN_OFF;
        }
        break;
    case STATUS_LED_WAITING_TO_TURN_OFF:
        if (deadline_passed(now_ms, led->deadline_ms)) {
            led->on = false;
            led->deadline_ms = now_ms + STATUS_LED_END_DELAY_MS;
            led->phase = STATUS_LED_CYCLE_END;
        }
        break;
    case STATUS_LED_CYCLE_END:
        if (deadline_passed(now_ms, led->deadline_ms)) {
            led->phase = STATUS_LED_CYCLE_START;
        }
        break;
    }
    return led->on;
}
