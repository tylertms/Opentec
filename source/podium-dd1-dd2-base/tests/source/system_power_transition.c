#include <assert.h>
#include <stdbool.h>

#include "system/power_transition.h"

static void test_waits_for_event_slot_and_ignores_matching_state(void) {
    SystemPowerTransition transition;
    SystemPowerTransitionAction action;
    system_power_transition_init(&transition);

    assert(!system_power_transition_update(&transition, false, true, 0, 0, &action));
    assert(!system_power_transition_update(&transition, true, false, 0, 0, &action));
    assert(!transition.applied_on);
    assert(action.pending_event_code == 0);
}

static void test_builds_standard_on_and_off_events(void) {
    SystemPowerTransition transition;
    SystemPowerTransitionAction action;
    system_power_transition_init(&transition);

    assert(system_power_transition_update(&transition, true, true, 1, 0, &action));
    assert(transition.applied_on);
    assert(action.pending_event_code == 0x0d);
    assert(action.active_event_code == 0x0d);
    assert(!action.status_update);
    assert(!action.feature_update);
    assert(!action.motor_control_update);

    assert(!system_power_transition_update(&transition, true, true, 1, 0, &action));
    assert(system_power_transition_update(&transition, false, true, 1, 0, &action));
    assert(!transition.applied_on);
    assert(action.pending_event_code == 0x1b);
    assert(action.active_event_code == 0x11);
    assert(!action.status_update);
    assert(!action.feature_update);
    assert(!action.motor_control_update);
}

static void test_builds_extended_mode_on_transition(void) {
    SystemPowerTransition transition;
    SystemPowerTransitionAction action;
    system_power_transition_init(&transition);

    assert(system_power_transition_update(&transition, true, true, 0x1c, 7, &action));
    assert(action.pending_event_code == 0x0d);
    assert(action.active_event_code == 0x0d);
    assert(action.feature_update);
    assert(action.feature_enabled);
    assert(action.status_update);
    assert(action.status_code == 0x2b);
    assert(!action.motor_control_update);
}

static void test_builds_extended_mode_idle_transition(void) {
    SystemPowerTransition transition = {.applied_on = true};
    SystemPowerTransitionAction action;

    assert(system_power_transition_update(&transition, false, true, 0x1c, 0, &action));
    assert(action.pending_event_code == 0x1b);
    assert(action.active_event_code == 0x11);
    assert(action.feature_update);
    assert(!action.feature_enabled);
    assert(action.status_update);
    assert(action.status_code == 0x1e);
    assert(!action.motor_control_update);
}

static void test_builds_extended_mode_active_transition(void) {
    SystemPowerTransition transition = {.applied_on = true};
    SystemPowerTransitionAction action;

    assert(system_power_transition_update(&transition, false, true, 0x1c, 1, &action));
    assert(action.feature_update);
    assert(!action.feature_enabled);
    assert(!action.status_update);
    assert(action.motor_control_update);
    assert(action.motor_control_state == 0x10);
}

int main(void) {
    test_waits_for_event_slot_and_ignores_matching_state();
    test_builds_standard_on_and_off_events();
    test_builds_extended_mode_on_transition();
    test_builds_extended_mode_idle_transition();
    test_builds_extended_mode_active_transition();
    return 0;
}
