#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/axis_override.h"

typedef struct {
    uint8_t mode;
    uint8_t operating_mode;
    uint8_t interface_mode;
    bool enabled;
    uint8_t calibration_value;
    uint8_t x;
    uint8_t y;
} TestInput;

static TestInput input(uint8_t mode) {
    TestInput value = {
        .mode = mode,
        .operating_mode = 0x13,
        .interface_mode = 0,
        .enabled = true,
        .calibration_value = 0x42,
        .x = 0x24,
        .y = 0x68,
    };
    return value;
}

static void process(WheelAxisOverrideProcessor *processor, const TestInput *value,
                    uint8_t axes[2]) {
    wheel_axis_override_process(processor, value->mode, value->operating_mode,
                                value->interface_mode, value->enabled, value->calibration_value,
                                value->x, value->y, axes);
}

static void test_selects_axis_overrides(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    uint8_t axes[2] = {0x31, 0xc2};

    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED);
    process(&processor, &value, axes);
    assert(processor.overrides.axis_7.enabled);
    assert(processor.overrides.axis_7.value == 0x42);
    assert(!processor.overrides.axis_5.enabled);
    assert(!processor.overrides.axis_6.enabled);
    assert(!processor.overrides.auxiliary.enabled);
    assert(axes[0] == 0x4e);
    assert(axes[1] == 0x42);

    value = input(WHEEL_AXIS_OVERRIDE_MODE_SECONDARY);
    process(&processor, &value, axes);
    assert(processor.overrides.axis_7.enabled);
    assert(processor.overrides.axis_7.value == 0x24);
    assert(processor.overrides.auxiliary.enabled);
    assert(processor.overrides.auxiliary.value == 0x68);
    assert(!processor.overrides.axis_5.enabled);
    assert(!processor.overrides.axis_6.enabled);

    value = input(WHEEL_AXIS_OVERRIDE_MODE_PRIMARY);
    process(&processor, &value, axes);
    assert(processor.overrides.axis_6.enabled);
    assert(processor.overrides.axis_6.value == 0x24);
    assert(processor.overrides.axis_5.enabled);
    assert(processor.overrides.axis_5.value == 0x68);
    assert(!processor.overrides.axis_7.enabled);
    assert(!processor.overrides.auxiliary.enabled);
}

static void test_handles_disabled_and_fixed_axis_modes(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    processor.x_available = true;
    processor.y_available = true;

    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_PRIMARY);
    value.enabled = false;
    uint8_t axes[2] = {0x31, 0xc2};
    process(&processor, &value, axes);
    assert(axes[0] == 0x4e);
    assert(axes[1] == 0x42);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_Y);
    assert(processor.x_available);
    assert(processor.y_available);

    value.enabled = true;
    value.operating_mode = 0x1c;
    process(&processor, &value, axes);
    assert(axes[0] == UINT8_MAX);
    assert(axes[1] == 0);

    value.enabled = false;
    axes[0] = 0x31;
    axes[1] = 0xc2;
    process(&processor, &value, axes);
    assert(axes[0] == 0x4e);
    assert(axes[1] == 0x42);

    value.enabled = true;
    value.mode = 0xff;
    axes[0] = 0x31;
    axes[1] = 0xc2;
    process(&processor, &value, axes);
    assert(axes[0] == 0x4e);
    assert(axes[1] == 0x42);
}

static void test_maps_direct_multiplexed_axes(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED);
    value.x = 0x87;
    value.y = 0x86;
    uint8_t axes[2] = {0x31, 0xc2};
    process(&processor, &value, axes);
    assert(axes[0] == 0x07);
    assert(axes[1] == 0x06);
    assert(processor.x_available);
    assert(!processor.y_available);
}

static void test_multiplexes_x_and_y_across_samples(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED);
    value.interface_mode = 6;
    value.x = 20;
    value.y = 40;
    uint8_t axes[2] = {0x31, 0xc2};

    process(&processor, &value, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_X);
    assert(axes[0] == 0x31);
    process(&processor, &value, axes);
    assert(axes[0] == 0x8a);

    value.x = UINT8_MAX;
    process(&processor, &value, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_SELECT);
    process(&processor, &value, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_Y);
    process(&processor, &value, axes);
    assert(axes[0] == 0x95);

    value.y = UINT8_MAX;
    process(&processor, &value, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_SELECT);
}

static void test_applies_interface_specific_multiplex_encoding(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED);
    value.interface_mode = 7;
    value.x = 20;
    value.y = UINT8_MAX;
    uint8_t axes[2] = {0x31, 0xc2};

    process(&processor, &value, axes);
    assert(axes[1] == 0);
    process(&processor, &value, axes);
    assert(axes[0] == 0x8b);
    assert(axes[1] == 0);

    wheel_axis_override_processor_init(&processor);
    value.interface_mode = 8;
    axes[0] = 0x31;
    process(&processor, &value, axes);
    process(&processor, &value, axes);
    assert(axes[0] == 10);
}

int main(void) {
    test_selects_axis_overrides();
    test_handles_disabled_and_fixed_axis_modes();
    test_maps_direct_multiplexed_axes();
    test_multiplexes_x_and_y_across_samples();
    test_applies_interface_specific_multiplex_encoding();
    return 0;
}
