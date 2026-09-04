#include <assert.h>
#include <stdbool.h>

#include "system/torque_transition.h"

static void test_waits_for_event_slot_and_ignores_matching_state(void) {
    SystemTorqueTransition transition;
    SystemTorqueTransitionAction action;
    system_torque_transition_init(&transition);

    assert(!system_torque_transition_update(&transition, false, true, 0, 0, &action));
    assert(!system_torque_transition_update(&transition, true, false, 0, 0, &action));
    assert(!transition.applied_disabled);
    assert(action.pending_event_code == 0);
}

static void test_builds_standard_disable_and_restore_events(void) {
    SystemTorqueTransition transition;
    SystemTorqueTransitionAction action;
    system_torque_transition_init(&transition);

    assert(system_torque_transition_update(&transition, true, true, 1, 0, &action));
    assert(!transition.applied_disabled);
    assert(action.pending_event_code == 0x0d);
    assert(action.active_event_code == 0x0d);
    assert(!action.status_update);
    assert(!action.feature_update);
    assert(!action.next_page_response);
    system_torque_transition_accept(&transition, true);
    assert(transition.applied_disabled);

    assert(!system_torque_transition_update(&transition, true, true, 1, 0, &action));
    assert(system_torque_transition_update(&transition, false, true, 1, 0, &action));
    assert(transition.applied_disabled);
    assert(action.pending_event_code == 0x1b);
    assert(action.active_event_code == 0x11);
    assert(!action.status_update);
    assert(!action.feature_update);
    assert(!action.next_page_response);
    system_torque_transition_accept(&transition, false);
    assert(!transition.applied_disabled);
}

static void test_builds_extended_mode_disable_transition(void) {
    SystemTorqueTransition transition;
    SystemTorqueTransitionAction action;
    system_torque_transition_init(&transition);

    assert(system_torque_transition_update(&transition, true, true, 0x1c, 7, &action));
    assert(!transition.applied_disabled);
    assert(action.pending_event_code == 0x0d);
    assert(action.active_event_code == 0x0d);
    assert(action.feature_update);
    assert(action.feature_enabled);
    assert(action.status_update);
    assert(action.status_code == 0x2b);
    assert(!action.next_page_response);
    system_torque_transition_accept(&transition, true);
}

static void test_builds_extended_mode_idle_transition(void) {
    SystemTorqueTransition transition = {.applied_disabled = true};
    SystemTorqueTransitionAction action;

    assert(system_torque_transition_update(&transition, false, true, 0x1c, 0, &action));
    assert(action.pending_event_code == 0x1b);
    assert(action.active_event_code == 0x11);
    assert(action.feature_update);
    assert(!action.feature_enabled);
    assert(action.status_update);
    assert(action.status_code == 0x1e);
    assert(!action.next_page_response);
    system_torque_transition_accept(&transition, false);
    assert(!transition.applied_disabled);
}

static void test_builds_extended_mode_active_transition(void) {
    SystemTorqueTransition transition = {.applied_disabled = true};
    SystemTorqueTransitionAction action;

    assert(system_torque_transition_update(&transition, false, true, 0x1c, 1, &action));
    assert(action.feature_update);
    assert(!action.feature_enabled);
    assert(!action.status_update);
    assert(action.next_page_response);
}

static void test_retries_until_event_acceptance(void) {
    SystemTorqueTransition transition;
    SystemTorqueTransitionAction action;
    system_torque_transition_init(&transition);

    assert(!system_torque_transition_update(&transition, true, false, 1, 0, &action));
    assert(!transition.applied_disabled);
    assert(system_torque_transition_update(&transition, true, true, 1, 0, &action));
    assert(!transition.applied_disabled);
    system_torque_transition_accept(&transition, true);
    assert(transition.applied_disabled);
    assert(!system_torque_transition_update(&transition, true, true, 1, 0, &action));
}

int main(void) {
    test_waits_for_event_slot_and_ignores_matching_state();
    test_builds_standard_disable_and_restore_events();
    test_builds_extended_mode_disable_transition();
    test_builds_extended_mode_idle_transition();
    test_builds_extended_mode_active_transition();
    test_retries_until_event_acceptance();
    return 0;
}
