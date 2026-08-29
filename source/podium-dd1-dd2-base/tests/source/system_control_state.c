#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "system/control_state.h"
#include "system/torque_transition.h"

static void test_initializes_baseline_state(void) {
    SystemControlState state;
    system_control_state_init(&state);

    assert(state.status_code == 0);
    assert(state.hid_configuration == 0x20);
    assert(state.hid_response_flags == 0);
    assert(state.active_event_code == 0);
    assert(state.motor_control_state == 0);
    assert(state.operating_status == 0);
    assert(state.operating_transition_code == 0);
    assert(!state.operating_feature_enabled);
}

static void test_normalizes_mode_eighteen_status_range(void) {
    SystemControlState state;
    system_control_state_init(&state);

    system_control_state_set_status(&state, 0x18, 0x017f);
    assert(state.status_code == 0x017f);
    system_control_state_set_status(&state, 0x18, 0x0180);
    assert(state.status_code == 0x13);
    system_control_state_set_status(&state, 0x18, 0x018f);
    assert(state.status_code == 0x13);
    system_control_state_set_status(&state, 0x18, 0x0190);
    assert(state.status_code == 0x0190);
    system_control_state_set_status(&state, 0x17, 0x0180);
    assert(state.status_code == 0x0180);
}

static void test_feature_state_owns_configuration_bit_zero(void) {
    SystemControlState state;
    system_control_state_init(&state);
    state.hid_configuration = 0xa5a4;

    system_control_state_set_operating_feature(&state, true);
    assert(state.operating_feature_enabled);
    assert(state.hid_configuration == 0xa5a5);
    system_control_state_set_operating_feature(&state, false);
    assert(!state.operating_feature_enabled);
    assert(state.hid_configuration == 0xa5a4);
}

static void test_motor_control_resets_transient_capabilities(void) {
    SystemControlState state;
    system_control_state_init(&state);
    state.hid_configuration = UINT16_MAX;

    system_control_state_set_motor_control(&state, 0x17, 0x10);
    assert(state.motor_control_state == 0x10);
    assert(state.hid_configuration == 0xff83);
    assert(state.hid_response_flags == 0x0002);

    state.operating_feature_enabled = true;
    state.hid_configuration = UINT16_MAX;
    system_control_state_set_motor_control(&state, 0x1c, 0x10);
    assert(state.hid_configuration == 0xff82);
    assert(!state.operating_feature_enabled);
}

static void test_applies_operating_status_by_wheel_mode(void) {
    SystemControlState state;
    system_control_state_init(&state);

    system_control_state_set_operating_status(&state, 0x0e, true);
    assert(state.operating_status == 1);
    assert(state.operating_transition_code == 2);
    assert(state.motor_control_state == 0);

    system_control_state_set_operating_status(&state, 0x0e, false);
    assert(state.operating_status == 0);
    assert(state.operating_transition_code == 0xff);

    system_control_state_set_operating_status(&state, 0x1c, true);
    assert(state.operating_status == 1);
    assert(state.motor_control_state == 2);
    assert(state.hid_response_flags == 2);

    system_control_state_set_operating_status(&state, 0x1c, false);
    assert(state.operating_status == 0);
    assert(state.motor_control_state == 0xff);
}

static void test_applies_extended_torque_transitions(void) {
    SystemControlState state;
    SystemTorqueTransition transition;
    SystemTorqueTransitionAction action;
    system_control_state_init(&state);
    system_torque_transition_init(&transition);

    assert(system_torque_transition_update(&transition, true, true, 0x1c, 7, &action));
    system_control_state_apply_torque_transition(&state, 0x1c, &action);
    assert(state.active_event_code == 0x0d);
    assert(state.status_code == 0x2b);
    assert(state.operating_feature_enabled);
    assert(state.hid_configuration == 0x21);

    assert(system_torque_transition_update(&transition, false, true, 0x1c, 1, &action));
    system_control_state_apply_torque_transition(&state, 0x1c, &action);
    assert(state.active_event_code == 0x11);
    assert(!state.operating_feature_enabled);
    assert(state.motor_control_state == 0x10);
    assert(state.hid_configuration == 0x00);
    assert(state.hid_response_flags == 0x0002);
}

int main(void) {
    test_initializes_baseline_state();
    test_normalizes_mode_eighteen_status_range();
    test_feature_state_owns_configuration_bit_zero();
    test_motor_control_resets_transient_capabilities();
    test_applies_operating_status_by_wheel_mode();
    test_applies_extended_torque_transitions();
    return 0;
}
