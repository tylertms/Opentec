#include <assert.h>
#include <stdint.h>

#include "wheel/compatibility_alert.h"

static void test_alternates_presentation_and_waits_for_event_slot(void) {
    WheelCompatibilityAlert alert;
    wheel_compatibility_alert_init(&alert);

    assert(wheel_compatibility_alert_update(&alert, true, false, 100) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_NONE);
    assert(wheel_compatibility_alert_update(&alert, true, true, 101) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED);
    assert(wheel_compatibility_alert_update(&alert, true, true, 1099) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_NONE);
    assert(wheel_compatibility_alert_update(&alert, true, false, 1100) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_NONE);
    assert(wheel_compatibility_alert_update(&alert, true, true, 1101) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_OUTLINED);
    assert(wheel_compatibility_alert_update(&alert, true, true, 2100) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED);
}

static void test_clears_when_supported(void) {
    WheelCompatibilityAlert alert;
    wheel_compatibility_alert_init(&alert);
    assert(wheel_compatibility_alert_update(&alert, false, true, 0) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_NONE);
    assert(wheel_compatibility_alert_update(&alert, true, true, 1) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED);
    assert(wheel_compatibility_alert_update(&alert, false, false, 2) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_CLEAR);
    assert(!alert.active);
}

static void test_blinks_segment_display(void) {
    assert(!wheel_compatibility_alert_segment_visible(0));
    assert(!wheel_compatibility_alert_segment_visible(124));
    assert(wheel_compatibility_alert_segment_visible(125));
    assert(wheel_compatibility_alert_segment_visible(249));
    assert(!wheel_compatibility_alert_segment_visible(250));
}

static void test_preserves_toggle_across_counter_wrap(void) {
    WheelCompatibilityAlert alert;
    wheel_compatibility_alert_init(&alert);
    assert(wheel_compatibility_alert_update(&alert, true, true, UINT32_MAX - 499) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED);
    assert(wheel_compatibility_alert_update(&alert, true, true, 500) ==
           WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_OUTLINED);
}

int main(void) {
    test_alternates_presentation_and_waits_for_event_slot();
    test_clears_when_supported();
    test_blinks_segment_display();
    test_preserves_toggle_across_counter_wrap();
    return 0;
}
