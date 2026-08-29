#include "motor/protocol.h"

#include <assert.h>

static void test_live_force(void) {
    MotorProtocolState state;
    motor_protocol_initialize(&state, 53U);

    MotorLinkFrame frame = {
        .type = MOTOR_LINK_FORCE_TYPE,
        .payload = {0x34U, 0x12U, 1U, 0xffU, 0xffU, 0x00U, 0x40U, 0U},
    };
    assert(motor_protocol_frame_apply(&state, &frame));
    assert(state.center == 0x1234);
    assert(state.live_drive_updated);
    assert(state.live_drive.primary_current == 17366);
    assert(state.live_drive.secondary_current == 8683);
    assert(state.live_drive.controller_coefficient == 0x9999U);

    state.live_drive_updated = false;
    state.status = 0x80U | 0x20U | 0x04U;
    assert(motor_protocol_frame_apply(&state, &frame));
    assert(state.live_drive_updated);
    assert(state.live_drive.primary_current == 32767);
    assert(state.live_drive.secondary_current == 0);
    assert(state.live_drive.controller_coefficient == 0x11c7U);
}

static void test_remote_effects(void) {
    MotorProtocolState state;
    motor_protocol_initialize(&state, 40U);
    state.status = 1U;

    MotorLinkFrame frame = {
        .type = MOTOR_LINK_FORCE_TYPE,
        .payload = {0x78U, 0x56U, 1U, 0xffU, 0xffU, 0U, 0U, 0U},
    };
    assert(motor_protocol_frame_apply(&state, &frame));
    assert(state.center == 0x5678);
    assert(!state.live_drive_updated);
}

static void test_status_command(void) {
    MotorProtocolState state;
    motor_protocol_initialize(&state, 53U);

    MotorLinkFrame frame = {
        .type = MOTOR_LINK_STATUS_TYPE,
        .payload = {0x83U, 0x01U, MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT, 0x80U, 0U, 0U, 0U},
    };
    assert(motor_protocol_frame_apply(&state, &frame));
    assert(state.status == 0x83U);
    assert(state.force_feedback.effects[0].active);
    assert(state.force_feedback.effects[0].type == MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT);

    frame.payload[1] = 0x11U;
    frame.payload[2] = 7U;
    assert(!motor_protocol_frame_apply(&state, &frame));

    frame.type = 9U;
    assert(!motor_protocol_frame_apply(&state, &frame));
}

static void test_replay_state(void) {
    MotorProtocolState state;
    motor_protocol_initialize(&state, 53U);
    MotorLinkFrame frame = {.type = MOTOR_LINK_FORCE_TYPE};

    assert(!motor_protocol_frame_result_apply(&state, MOTOR_LINK_FRAME_INVALID_BOUNDARY, &frame));
    assert(state.replay);
    assert(!motor_protocol_frame_result_apply(&state, MOTOR_LINK_FRAME_INVALID_CHECKSUM, &frame));
    assert(state.replay);
    assert(motor_protocol_frame_result_apply(&state, MOTOR_LINK_FRAME_VALID, &frame));
    assert(!state.replay);
}

static void test_local_force_feedback_service(void) {
    MotorProtocolState state;
    motor_protocol_initialize(&state, 53U);

    MotorLinkFrame frame = {
        .type = MOTOR_LINK_STATUS_TYPE,
        .payload = {0x03U, 0x01U, MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT, 0U, 0U, 0U, 0U, 0U},
    };
    assert(motor_protocol_frame_apply(&state, &frame));
    assert(!motor_protocol_force_feedback_service(&state, 0U, 0, 0));
    assert(!motor_protocol_force_feedback_service(&state, 9U, 0, 0));
    assert(motor_protocol_force_feedback_service(&state, 10U, 0, 0));
    assert(state.live_drive_updated);
    assert(state.live_drive.primary_current == 6078);
    assert(!motor_protocol_force_feedback_service(&state, 10U, 0, 0));
    assert(motor_protocol_force_feedback_service(&state, 11U, 0, 0));
}

static void test_local_force_feedback_gates(void) {
    MotorProtocolState state;
    motor_protocol_initialize(&state, 40U);
    state.force_feedback.effects[0].active = true;
    state.status = 0x05U;
    assert(!motor_protocol_force_feedback_service(&state, 0U, 0, 0));
    assert(!state.force_feedback.effects[0].active);
    assert(state.force_feedback.ramp_percent == 0U);

    state.status = 0x03U;
    assert(!motor_protocol_force_feedback_service(&state, 1U, 0, 0));
    assert(state.force_feedback.ramp_percent == 0U);
    assert(motor_protocol_force_feedback_service(&state, 51U, 0, 0));
    assert(state.force_feedback.ramp_percent == 1U);
    assert(motor_protocol_force_feedback_service(&state, 101U, 0, 0));
    assert(state.force_feedback.ramp_percent == 1U);
    assert(motor_protocol_force_feedback_service(&state, 102U, 0, 0));
    assert(state.force_feedback.ramp_percent == 2U);

    state.force_feedback.effects[1].active = true;
    state.status = 0x43U;
    assert(motor_protocol_force_feedback_service(&state, 103U, 0, 0));
    assert(!state.force_feedback.effects[1].active);
    assert(state.force_feedback.ramp_percent == 2U);
}

int main(void) {
    test_live_force();
    test_remote_effects();
    test_status_command();
    test_replay_state();
    test_local_force_feedback_service();
    test_local_force_feedback_gates();
    return 0;
}
