#include "board/status_led.h"

#include <assert.h>
#include <stdint.h>

static void test_repeating_active_low_pulse_timing(void) {
    StatusLed led;
    status_led_init(&led);

    assert(!status_led_update(&led, 100));
    assert(!status_led_update(&led, 102));
    assert(status_led_update(&led, 103));
    assert(status_led_update(&led, 104));
    assert(!status_led_update(&led, 105));
    assert(!status_led_update(&led, 106));
    assert(!status_led_update(&led, 107));
    assert(led.phase == STATUS_LED_CYCLE_START);

    assert(!status_led_update(&led, 108));
    assert(!status_led_update(&led, 110));
    assert(status_led_update(&led, 111));
}

static void test_deadline_wraparound(void) {
    StatusLed led;
    status_led_init(&led);

    assert(!status_led_update(&led, UINT32_MAX - 1));
    assert(!status_led_update(&led, 0));
    assert(status_led_update(&led, 1));
}

int main(void) {
    test_repeating_active_low_pulse_timing();
    test_deadline_wraparound();
    return 0;
}
