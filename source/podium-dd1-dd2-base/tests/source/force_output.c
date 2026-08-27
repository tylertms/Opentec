#include <assert.h>
#include <stdint.h>

#include "force_feedback/output.h"

static void test_slew_and_direction(void) {
    ForceOutputState state;
    const ForceOutputLimits limits = {
        .maximum_magnitude = 1000,
        .maximum_step = 100,
    };

    force_output_reset(&state);
    ForceOutputCommand command = force_output_step(&state, 500, 1, &limits);
    assert(command.magnitude == 100);
    assert(command.negative == 0);
    assert(command.active == 1);

    command = force_output_step(&state, 500, 1, &limits);
    assert(command.magnitude == 200);
    command = force_output_step(&state, -500, 1, &limits);
    assert(command.magnitude == 100);
    assert(command.negative == 0);
    command = force_output_step(&state, -500, 1, &limits);
    assert(command.magnitude == 0);
    command = force_output_step(&state, -500, 1, &limits);
    assert(command.magnitude == 100);
    assert(command.negative == 1);
}

static void test_saturation(void) {
    ForceOutputState state;
    const ForceOutputLimits limits = {
        .maximum_magnitude = 5000,
        .maximum_step = 0,
    };

    force_output_reset(&state);
    ForceOutputCommand command = force_output_step(&state, INT32_MAX, 1, &limits);
    assert(command.magnitude == 5000);
    assert(command.negative == 0);
    command = force_output_step(&state, INT32_MIN, 1, &limits);
    assert(command.magnitude == 5000);
    assert(command.negative == 1);
}

static void test_interlock(void) {
    ForceOutputState state = {.value = -1000};
    const ForceOutputLimits limits = {
        .maximum_magnitude = UINT16_MAX,
        .maximum_step = 1,
    };

    ForceOutputCommand command = force_output_step(&state, INT32_MIN, 0, &limits);
    assert(state.value == 0);
    assert(command.magnitude == 0);
    assert(command.active == 0);
    command = force_output_step(&state, 100, 1, &(ForceOutputLimits){0});
    assert(command.active == 0);
}

int main(void) {
    test_slew_and_direction();
    test_saturation();
    test_interlock();
    return 0;
}
