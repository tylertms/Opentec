#include "wheel/compatibility_alert.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_COMPATIBILITY_ICON_INTERVAL_MS = 1000,
    WHEEL_COMPATIBILITY_SEGMENT_PERIOD_MS = 250,
    WHEEL_COMPATIBILITY_SEGMENT_OFF_MS = 125,
};

/**
 * @brief Initializes the unsupported-wheel alert.
 *
 * Starts inactive with no pending presentation change.
 *
 * @param[out] alert Compatibility alert to initialize.
 */
void wheel_compatibility_alert_init(WheelCompatibilityAlert *alert) {
    *alert = (WheelCompatibilityAlert){0};
}

/**
 * @brief Advances unsupported-wheel presentation.
 *
 * Starts with the inverted warning, alternates inverted and outlined warnings once per second,
 * and retains a presentation change until the shared event slot is available. Leaving the
 * unsupported state requests immediate removal.
 *
 * @param[in,out] alert Compatibility alert state.
 * @param[in] unsupported True while the attached wheel selected an unsupported protocol mode.
 * @param[in] event_slot_available True when a presentation event can be accepted.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Presentation action for the firmware integration layer.
 */
WheelCompatibilityAlertAction wheel_compatibility_alert_update(WheelCompatibilityAlert *alert,
                                                               bool unsupported,
                                                               bool event_slot_available,
                                                               uint32_t now_ms) {
    if (!unsupported) {
        if (!alert->active) {
            return WHEEL_COMPATIBILITY_ALERT_ACTION_NONE;
        }
        wheel_compatibility_alert_init(alert);
        return WHEEL_COMPATIBILITY_ALERT_ACTION_CLEAR;
    }

    if (!alert->active) {
        alert->active = true;
        alert->presentation_pending = true;
        alert->next_toggle_ms = now_ms + WHEEL_COMPATIBILITY_ICON_INTERVAL_MS;
    } else if ((int32_t)(now_ms - alert->next_toggle_ms) >= 0) {
        alert->outlined = !alert->outlined;
        alert->presentation_pending = true;
        alert->next_toggle_ms = now_ms + WHEEL_COMPATIBILITY_ICON_INTERVAL_MS;
    }

    if (!alert->presentation_pending || !event_slot_available) {
        return WHEEL_COMPATIBILITY_ALERT_ACTION_NONE;
    }
    alert->presentation_pending = false;
    return alert->outlined ? WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_OUTLINED
                           : WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED;
}

/**
 * @brief Selects the unsupported-wheel segment-display blink phase.
 *
 * Keeps the display blank for the first 125 milliseconds of each 250-millisecond period and
 * visible for the remaining interval.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True while the firmware-update-required glyphs should be visible.
 */
bool wheel_compatibility_alert_segment_visible(uint32_t now_ms) {
    return now_ms % WHEEL_COMPATIBILITY_SEGMENT_PERIOD_MS >= WHEEL_COMPATIBILITY_SEGMENT_OFF_MS;
}
