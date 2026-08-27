#include <assert.h>
#include <stdint.h>

#include "pedal/analog.h"

static void test_detects_analog_connector_level(void) {
    assert(pedal_analog_detect(0x0fff));
    assert(!pedal_analog_detect(0));
}

static void test_scales_inverted_adc_samples(void) {
    PedalAnalog analog;
    PedalInput input;
    pedal_analog_init(&analog);

    const uint16_t released[PEDAL_INPUT_AXIS_COUNT] = {0, 0, 0};
    pedal_analog_update(&analog, released, &input);
    assert(input.axes[0] == UINT16_MAX);
    assert(input.axes[1] == UINT16_MAX);
    assert(input.axes[2] == UINT16_MAX);

    const uint16_t pressed[PEDAL_INPUT_AXIS_COUNT] = {0x0fff, 0x0fff, 0x0fff};
    pedal_analog_update(&analog, pressed, &input);
    assert(input.axes[0] == 0);
    assert(input.axes[1] == 0);
    assert(input.axes[2] == 0);
    assert(input.auxiliary == 0);
}

int main(void) {
    test_detects_analog_connector_level();
    test_scales_inverted_adc_samples();
    return 0;
}
