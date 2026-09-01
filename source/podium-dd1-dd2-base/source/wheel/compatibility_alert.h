#ifndef OPENTEC_BASE_WHEEL_COMPATIBILITY_ALERT_H
#define OPENTEC_BASE_WHEEL_COMPATIBILITY_ALERT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Display action emitted by the unsupported-wheel compatibility alert.
 *
 * Actions describe one presentation change for the shared display event slot.
 */
typedef enum {
    WHEEL_COMPATIBILITY_ALERT_ACTION_NONE,          /**< No display change is requested. */
    WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED, /**< Show the inverted warning. */
    WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_OUTLINED, /**< Show the outlined warning. */
    WHEEL_COMPATIBILITY_ALERT_ACTION_CLEAR,         /**< Clear the active warning. */
} WheelCompatibilityAlertAction;

/**
 * @brief Persistent unsupported-wheel compatibility alert state.
 *
 * The state retains the warning presentation, its one-second toggle deadline, and any display
 * change waiting for the shared event slot.
 */
typedef struct {
    uint32_t next_toggle_ms; /**< Earliest time at which the warning presentation may toggle. */
    bool active;             /**< True while the unsupported-wheel alert is active. */
    bool outlined; /**< True when the current or queued warning presentation is outlined. */
    bool presentation_pending; /**< True when a presentation change awaits the event slot. */
} WheelCompatibilityAlert;

/**
 * @brief Initializes unsupported-wheel compatibility alert state.
 *
 * Clears active presentation, toggle timing, and pending display work.
 *
 * @param[out] alert Compatibility alert state to initialize.
 */
void wheel_compatibility_alert_init(WheelCompatibilityAlert *alert);

/**
 * @brief Advances unsupported-wheel compatibility presentation.
 *
 * Starts with an inverted warning, toggles its outline once per second, retains changes while the
 * event slot is unavailable, and requests clear when unsupported mode ends.
 *
 * @param[in,out] alert Compatibility alert state.
 * @param[in] unsupported true while the attached wheel selected an unsupported protocol mode.
 * @param[in] event_slot_available true when a presentation event can be accepted.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The next display action, or WHEEL_COMPATIBILITY_ALERT_ACTION_NONE when no action is due.
 */
WheelCompatibilityAlertAction wheel_compatibility_alert_update(WheelCompatibilityAlert *alert,
                                                               bool unsupported,
                                                               bool event_slot_available,
                                                               uint32_t now_ms);

/**
 * @brief Tests the unsupported-wheel segment-display blink phase.
 *
 * Keeps the display blank for the first 125 milliseconds of each 250-millisecond period and
 * visible for the remaining interval.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return true when the warning glyphs should be visible; false during the blank interval.
 */
bool wheel_compatibility_alert_segment_visible(uint32_t now_ms);

#endif
