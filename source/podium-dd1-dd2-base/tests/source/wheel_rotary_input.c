#include <assert.h>
#include <stdint.h>

#include "wheel/rotary_input.h"

static void test_initializes_unsynchronized_channels(void) {
    WheelRotaryInput input;

    wheel_rotary_input_init(&input);

    for (uint8_t channel = 0; channel < WHEEL_ROTARY_INPUT_CHANNEL_COUNT; channel++) {
        assert(input.channels[channel].position == UINT8_MAX);
        assert(input.channels[channel].pending_steps == 0);
        assert(input.channels[channel].event == WHEEL_ROTARY_EVENT_NONE);
        assert(input.channels[channel].phase == WHEEL_ROTARY_PHASE_IDLE);
    }
}

static void test_emits_forward_hold_and_release_intervals(void) {
    WheelRotaryInput input;
    wheel_rotary_input_init(&input);

    assert(wheel_rotary_input_update(&input, 0, 5, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, 0, 6, 1) == WHEEL_ROTARY_EVENT_FORWARD);
    assert(wheel_rotary_input_update(&input, 0, 6, 80) == WHEEL_ROTARY_EVENT_FORWARD);
    assert(wheel_rotary_input_update(&input, 0, 6, 81) == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[0].phase == WHEEL_ROTARY_PHASE_RELEASE);
    assert(wheel_rotary_input_update(&input, 0, 6, 160) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, 0, 6, 161) == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[0].phase == WHEEL_ROTARY_PHASE_IDLE);
}

static void test_emits_backward_event(void) {
    WheelRotaryInput input;
    wheel_rotary_input_init(&input);

    assert(wheel_rotary_input_update(&input, 0, 8, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, 0, 7, 1) == WHEEL_ROTARY_EVENT_BACKWARD);
}

static void test_tracks_twelve_position_wrap(void) {
    WheelRotaryInput input;
    wheel_rotary_input_init(&input);

    assert(wheel_rotary_input_update(&input, 0, 12, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, 0, 1, 1) == WHEEL_ROTARY_EVENT_FORWARD);
    assert(input.channels[0].position == 1);

    wheel_rotary_input_init(&input);
    assert(wheel_rotary_input_update(&input, 0, 1, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, 0, 12, 1) == WHEEL_ROTARY_EVENT_BACKWARD);
    assert(input.channels[0].position == 12);
}

static void test_advances_one_position_per_update(void) {
    WheelRotaryInput input;
    wheel_rotary_input_init(&input);

    assert(wheel_rotary_input_update(&input, 0, 2, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, 0, 5, 1) == WHEEL_ROTARY_EVENT_FORWARD);
    assert(input.channels[0].position == 3);
    assert(wheel_rotary_input_update(&input, 0, 5, 2) == WHEEL_ROTARY_EVENT_FORWARD);
    assert(input.channels[0].position == 4);
    assert(input.channels[0].pending_steps == 1);
}

static void test_hold_expiry_clears_all_pending_steps(void) {
    WheelRotaryInput input;
    wheel_rotary_input_init(&input);
    wheel_rotary_input_update(&input, 0, 1, 0);
    wheel_rotary_input_update(&input, 1, 1, 0);
    wheel_rotary_input_update(&input, 0, 2, 1);
    wheel_rotary_input_update(&input, 1, 2, 1);
    wheel_rotary_input_update(&input, 1, 3, 2);
    assert(input.channels[1].pending_steps == 1);

    assert(wheel_rotary_input_update(&input, 0, 2, 81) == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[0].pending_steps == 0);
    assert(input.channels[1].pending_steps == 0);
}

static void test_ignores_zero_and_invalid_channels(void) {
    WheelRotaryInput input;
    wheel_rotary_input_init(&input);

    assert(wheel_rotary_input_update(&input, 0, 0, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[0].position == UINT8_MAX);
    assert(wheel_rotary_input_update(NULL, 0, 1, 0) == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_rotary_input_update(&input, WHEEL_ROTARY_INPUT_CHANNEL_COUNT, 1, 0) ==
           WHEEL_ROTARY_EVENT_NONE);
}

int main(void) {
    test_initializes_unsynchronized_channels();
    test_emits_forward_hold_and_release_intervals();
    test_emits_backward_event();
    test_tracks_twelve_position_wrap();
    test_advances_one_position_per_update();
    test_hold_expiry_clears_all_pending_steps();
    test_ignores_zero_and_invalid_channels();
    return 0;
}
