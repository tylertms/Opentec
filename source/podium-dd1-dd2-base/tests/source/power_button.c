#include "board/power_button.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static void test_debounces_and_blanks_shutdown_display(void) {
    PowerButton button;
    power_button_init(&button);

    assert(power_button_update(&button, false, true, 100) == POWER_BUTTON_ACTION_NONE);
    assert(!button.active);
    assert(power_button_update(&button, true, true, 299) == POWER_BUTTON_ACTION_NONE);
    assert(!button.active);

    PowerButtonAction actions = power_button_update(&button, true, true, 300);
    assert(actions == (POWER_BUTTON_ACTION_SHUTDOWN | POWER_BUTTON_ACTION_BLANK_DISPLAY));
    assert(button.active);
    assert(button.shutdown_started);

    assert(power_button_update(&button, true, true, 2299) == POWER_BUTTON_ACTION_BLANK_DISPLAY);
    assert(power_button_update(&button, true, true, 2300) == POWER_BUTTON_ACTION_BLANK_DISPLAY);
    assert(power_button_update(&button, true, true, 2301) == POWER_BUTTON_ACTION_CLEAR_DISPLAY);
    assert(power_button_update(&button, true, true, 2302) == POWER_BUTTON_ACTION_NONE);
}

static void test_release_rearms_and_clears_after_original_deadline(void) {
    PowerButton button;
    power_button_init(&button);

    power_button_update(&button, false, true, 0);
    power_button_update(&button, true, true, 200);
    assert(power_button_update(&button, false, true, 300) == POWER_BUTTON_ACTION_NONE);
    assert(!button.active);
    assert(!button.shutdown_started);
    assert(power_button_update(&button, false, true, 2201) == POWER_BUTTON_ACTION_CLEAR_DISPLAY);

    assert(power_button_update(&button, true, true, 2400) == POWER_BUTTON_ACTION_NONE);
    assert(power_button_update(&button, true, true, 2401) ==
           (POWER_BUTTON_ACTION_SHUTDOWN | POWER_BUTTON_ACTION_BLANK_DISPLAY));
}

static void test_disabled_action_preserves_latch_until_release(void) {
    PowerButton button;
    power_button_init(&button);

    power_button_update(&button, false, true, 0);
    power_button_update(&button, true, true, 200);
    assert(power_button_update(&button, true, false, 201) == POWER_BUTTON_ACTION_NONE);
    assert(button.active);
    assert(button.shutdown_started);
    assert(power_button_update(&button, true, false, 2201) == POWER_BUTTON_ACTION_CLEAR_DISPLAY);
    assert(button.active);
    power_button_update(&button, false, false, 2202);
    assert(!button.active);
    assert(!button.shutdown_started);
}

static void test_deadlines_survive_counter_wrap(void) {
    PowerButton button;
    power_button_init(&button);

    power_button_update(&button, false, true, UINT32_MAX - 99);
    assert(power_button_update(&button, true, true, 100) ==
           (POWER_BUTTON_ACTION_SHUTDOWN | POWER_BUTTON_ACTION_BLANK_DISPLAY));
    assert(power_button_update(&button, true, true, 2100) == POWER_BUTTON_ACTION_BLANK_DISPLAY);
    assert(power_button_update(&button, true, true, 2101) == POWER_BUTTON_ACTION_CLEAR_DISPLAY);
}

int main(void) {
    test_debounces_and_blanks_shutdown_display();
    test_release_rearms_and_clears_after_original_deadline();
    test_disabled_action_preserves_latch_until_release();
    test_deadlines_survive_counter_wrap();
    return 0;
}
