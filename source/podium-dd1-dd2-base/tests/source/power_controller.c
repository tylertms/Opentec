#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "board/power.h"

static PowerAction update(PowerController *controller, bool pressed, bool enabled,
                          uint32_t now_ms) {
    return power_controller_update(controller, pressed, enabled, now_ms);
}

static void start(PowerController *controller, uint32_t now_ms) {
    power_controller_init(controller);
    assert(update(controller, false, true, now_ms) == POWER_ACTION_NONE);
    assert(update(controller, true, true, now_ms) == POWER_ACTION_ENABLE_LATCH);
    assert(controller->phase == POWER_PHASE_READY);
}

static void test_waits_for_start_button_and_respects_control_gate(void) {
    PowerController controller;
    power_controller_init(&controller);

    assert(update(&controller, false, true, 100) == POWER_ACTION_NONE);
    assert(controller.phase == POWER_PHASE_WAITING_FOR_START);
    assert(update(&controller, true, true, 101) == POWER_ACTION_ENABLE_LATCH);
    assert(update(&controller, true, false, 102) == POWER_ACTION_NONE);
    assert(controller.phase == POWER_PHASE_READY);
    assert(update(&controller, true, false, 5000) == POWER_ACTION_NONE);
    assert(update(&controller, true, true, 5001) == POWER_ACTION_NONE);
    assert(controller.phase == POWER_PHASE_BUTTON_HELD);
    assert(update(&controller, true, true, 6001) == POWER_ACTION_NONE);
    assert(update(&controller, true, true, 6002) == POWER_ACTION_BEGIN_SHUTDOWN);
}

static void test_short_release_toggles_requested_state(void) {
    PowerController controller;
    start(&controller, 100);

    assert(update(&controller, true, true, 101) == POWER_ACTION_NONE);
    assert(controller.phase == POWER_PHASE_BUTTON_HELD);
    assert(update(&controller, false, true, 1101) == POWER_ACTION_TORQUE_REQUEST_CHANGED);
    assert(controller.torque_disabled);

    assert(update(&controller, true, true, 1200) == POWER_ACTION_NONE);
    assert(update(&controller, false, true, 1201) == POWER_ACTION_TORQUE_REQUEST_CHANGED);
    assert(!controller.torque_disabled);
}

static void test_release_precedes_expired_hold(void) {
    PowerController controller;
    start(&controller, 0);
    assert(update(&controller, true, true, 10) == POWER_ACTION_NONE);

    assert(update(&controller, false, true, 1011) == POWER_ACTION_TORQUE_REQUEST_CHANGED);
    assert(controller.phase == POWER_PHASE_READY);
    assert(controller.torque_disabled);
}

static void test_strict_shutdown_boundaries(void) {
    PowerController controller;
    start(&controller, 0);
    assert(update(&controller, true, true, 10) == POWER_ACTION_NONE);

    assert(update(&controller, true, true, 1010) == POWER_ACTION_NONE);
    assert(update(&controller, true, true, 1011) == POWER_ACTION_BEGIN_SHUTDOWN);
    assert(controller.phase == POWER_PHASE_SHUTDOWN_DELAY);
    assert(!controller.torque_disabled);

    assert(update(&controller, true, true, 2011) == POWER_ACTION_NONE);
    assert(update(&controller, true, true, 2012) == POWER_ACTION_FINISH_SHUTDOWN);
    assert(controller.phase == POWER_PHASE_OFF);
    assert(update(&controller, false, true, 3000) == POWER_ACTION_NONE);
}

static void test_deadlines_survive_counter_wrap(void) {
    PowerController controller;
    start(&controller, UINT32_MAX - 600);
    assert(update(&controller, true, true, UINT32_MAX - 500) == POWER_ACTION_NONE);

    assert(update(&controller, true, true, 499) == POWER_ACTION_NONE);
    assert(update(&controller, true, true, 500) == POWER_ACTION_BEGIN_SHUTDOWN);
    assert(update(&controller, true, true, 1500) == POWER_ACTION_NONE);
    assert(update(&controller, true, true, 1501) == POWER_ACTION_FINISH_SHUTDOWN);
}

int main(void) {
    test_waits_for_start_button_and_respects_control_gate();
    test_short_release_toggles_requested_state();
    test_release_precedes_expired_hold();
    test_strict_shutdown_boundaries();
    test_deadlines_survive_counter_wrap();
    return 0;
}
