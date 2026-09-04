#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "system/control_state.h"
#include "system/torque_transition.h"

static void test_initializes_baseline_state(void) {
    SystemControlState state;
    system_control_state_init(&state);

    assert(state.status_code == UINT16_MAX);
    assert(state.wheel_response.code == REMOTE_TUNING_RESPONSE_NONE);
    assert(state.active_event_code == 0);
    assert(state.operating_status == 0);
    assert(!state.operating_feature_enabled);
}

static void test_takes_each_status_once(void) {
    SystemControlState state;
    uint16_t code = 0;
    system_control_state_init(&state);

    assert(!system_control_state_take_status(&state, &code));
    system_control_state_set_status(&state, 0x1c, 0x012b);
    assert(system_control_state_take_status(&state, &code));
    assert(code == 0x012b);
    assert(!system_control_state_take_status(&state, &code));
    assert(state.status_code == UINT16_MAX);
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

static void test_retains_operating_feature_state(void) {
    SystemControlState state;
    system_control_state_init(&state);

    system_control_state_set_operating_feature(&state, true);
    assert(state.operating_feature_enabled);
    system_control_state_set_operating_feature(&state, false);
    assert(!state.operating_feature_enabled);
}

static void test_retains_active_event_code(void) {
    SystemControlState state;
    uint8_t display_state = 0;
    system_control_state_init(&state);

    system_control_state_set_active_event(&state, 5);

    assert(state.active_event_code == 5);
    assert(system_control_state_take_display_state(&state, &display_state));
    assert(display_state == 5);
    assert(!system_control_state_take_display_state(&state, &display_state));

    system_control_state_set_active_event(&state, 6);
    system_control_state_set_active_event(&state, 7);
    assert(system_control_state_take_display_state(&state, &display_state));
    assert(display_state == 7);
}

static void test_takes_each_wheel_response_once(void) {
    SystemControlState state;
    RemoteTuningResponse response;
    system_control_state_init(&state);

    assert(!system_control_state_take_wheel_response(&state, &response));
    system_control_state_set_operating_status(&state, 0x1c, true);
    assert(system_control_state_take_wheel_response(&state, &response));
    assert(response.link == REMOTE_TUNING_LINK_EXTENDED);
    assert(response.code == REMOTE_TUNING_RESPONSE_ACTIVE);
    assert(!system_control_state_take_wheel_response(&state, &response));
}

static void test_applies_operating_status_by_wheel_mode(void) {
    SystemControlState state;
    system_control_state_init(&state);

    system_control_state_set_operating_status(&state, 0x0e, true);
    assert(state.operating_status == 1);
    assert(state.wheel_response.link == REMOTE_TUNING_LINK_LEGACY);
    assert(state.wheel_response.code == REMOTE_TUNING_RESPONSE_ACTIVE);

    system_control_state_set_operating_status(&state, 0x0e, false);
    assert(state.operating_status == 0);
    assert(state.wheel_response.link == REMOTE_TUNING_LINK_LEGACY);
    assert(state.wheel_response.code == REMOTE_TUNING_RESPONSE_INACTIVE);

    system_control_state_set_operating_status(&state, 0x1c, true);
    assert(state.operating_status == 1);
    assert(state.wheel_response.code == REMOTE_TUNING_RESPONSE_ACTIVE);
    assert(!state.operating_feature_enabled);

    system_control_state_set_operating_status(&state, 0x1c, false);
    assert(state.operating_status == 0);
    assert(state.wheel_response.code == REMOTE_TUNING_RESPONSE_INACTIVE);
}

static void test_applies_extended_torque_transitions(void) {
    SystemControlState state;
    SystemTorqueTransition transition;
    SystemTorqueTransitionAction action;
    system_control_state_init(&state);
    system_torque_transition_init(&transition);

    assert(system_torque_transition_update(&transition, true, true, 0x1c, 7, &action));
    system_torque_transition_accept(&transition, true);
    system_control_state_apply_torque_transition(&state, 0x1c, 4, &action);
    assert(state.active_event_code == 0x0d);
    assert(state.status_code == 0x2b);
    assert(state.operating_feature_enabled);

    assert(system_torque_transition_update(&transition, false, true, 0x1c, 1, &action));
    system_torque_transition_accept(&transition, false);
    system_control_state_apply_torque_transition(&state, 0x1c, 4, &action);
    assert(state.active_event_code == 0x11);
    assert(!state.operating_feature_enabled);
    assert(state.wheel_response.link == REMOTE_TUNING_LINK_EXTENDED);
    assert(state.wheel_response.code == REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE);
    assert(state.wheel_response.value == 4);
}

int main(void) {
    test_initializes_baseline_state();
    test_takes_each_status_once();
    test_normalizes_mode_eighteen_status_range();
    test_retains_operating_feature_state();
    test_retains_active_event_code();
    test_takes_each_wheel_response_once();
    test_applies_operating_status_by_wheel_mode();
    test_applies_extended_torque_transitions();
    return 0;
}
