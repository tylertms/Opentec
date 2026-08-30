#include <assert.h>
#include <stdint.h>

#include "pedal/analog.h"

static void test_initializes_calibration_defaults(void) {
    PedalAnalog analog;
    pedal_analog_init(&analog);

    assert(!analog.active);
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        assert(analog.axes[axis].minimum == 0);
        assert(analog.axes[axis].maximum == 0);
        assert(analog.axes[axis].lower_deadzone == 45);
        assert(analog.axes[axis].upper_deadzone == 120);
        assert(analog.axes[axis].output_scale == UINT16_MAX);
        assert(!analog.axes[axis].learn_minimum);
        assert(analog.axes[axis].learn_maximum);
    }
}

static void test_captures_and_scales_analog_samples(void) {
    PedalAnalog analog;
    PedalInput input = {0};
    pedal_analog_init(&analog);

    const uint16_t disconnected[PEDAL_INPUT_AXIS_COUNT] = {0, 0, 0};
    assert(!pedal_analog_update(&analog, disconnected, &input));

    const uint16_t samples[PEDAL_INPUT_AXIS_COUNT] = {0, 0x0800, 0x0fff};
    assert(pedal_analog_update(&analog, samples, &input));
    assert(analog.active);
    assert(analog.axes[0].minimum == 0x0ffe);
    assert(analog.axes[1].minimum == 0x07fe);
    assert(analog.axes[2].minimum == 0);
    assert(input.axes[0] == 0);
    assert(input.axes[1] == 0);
    assert(input.axes[2] == 0);

    assert(pedal_analog_update(&analog, samples, &input));
    assert(input.axes[0] == 45);
    assert(input.axes[1] == 0);
    assert(input.axes[2] == 0);
    assert(input.auxiliary == 0);

    assert(!pedal_analog_update(&analog, disconnected, &input));
    assert(!analog.active);
    assert(input.axes[0] == 0);
    assert(input.axes[1] == 0);
    assert(input.axes[2] == 0);
}

static void test_detects_analog_fallback_from_third_channel(void) {
    const uint16_t equal_limit[PEDAL_INPUT_AXIS_COUNT] = {0x0800, 0, 0x0a2f};
    const uint16_t below_limit[PEDAL_INPUT_AXIS_COUNT] = {0, 0, 0x0a31};

    assert(!pedal_analog_detect(equal_limit));
    assert(pedal_analog_detect(below_limit));
}

int main(void) {
    test_initializes_calibration_defaults();
    test_captures_and_scales_analog_samples();
    test_detects_analog_fallback_from_third_channel();
    return 0;
}
