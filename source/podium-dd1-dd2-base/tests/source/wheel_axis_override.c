#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/axis_override.h"

typedef struct {
    uint8_t mode;
    uint8_t operating_mode;
    uint8_t interface_mode;
    bool enabled;
    uint8_t bite_point_percent;
    uint32_t now_ms;
    uint8_t buttons;
    int8_t motion;
    uint8_t x;
    uint8_t y;
} TestInput;

static TestInput input(uint8_t mode) {
    TestInput value = {
        .mode = mode,
        .operating_mode = 0x13,
        .interface_mode = 0,
        .enabled = true,
        .bite_point_percent = 50,
        .now_ms = 0,
        .buttons = 0,
        .motion = 0,
        .x = 0x24,
        .y = 0x68,
    };
    return value;
}

static void process(WheelAxisOverrideProcessor *processor, TestInput *value, uint8_t axes[2]) {
    wheel_axis_override_process(processor, value->mode, value->operating_mode,
                                value->interface_mode, value->enabled, value->now_ms,
                                &value->bite_point_percent, &value->buttons, &value->motion,
                                value->x, value->y, axes);
}

static void process_packet(WheelAxisOverrideProcessor *processor, uint8_t mode, uint8_t wheel_mode,
                           uint8_t interface_mode, uint8_t axis_limit, uint8_t bite_point_percent,
                           uint8_t controls[8], uint8_t axes[2]) {
    uint8_t buttons = 0;
    int8_t motion = 0;
    wheel_axis_override_process_packet(processor, mode, wheel_mode, interface_mode, axis_limit, 0,
                                       &bite_point_percent, &buttons, &motion, controls, axes);
}

static void process_axis_mode(WheelAxisOverrideProcessor *processor, uint8_t mode,
                              uint8_t interface_mode, uint8_t controls[8], uint8_t axes[2]) {
    uint8_t bite_point_percent = 50;
    uint8_t buttons = 0;
    int8_t motion = 0;
    wheel_axis_override_process_axis_mode(processor, mode, interface_mode, 0, &bite_point_percent,
                                          &buttons, &motion, controls, axes);
}

static void test_adjusts_and_publishes_paddle_clutch_bite_point(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED);
    uint8_t axes[2] = {0};

    value.x = 0;
    value.y = 100;
    value.buttons = UINT8_MAX;
    process(&processor, &value, axes);
    assert(processor.paddle_clutch_phase == WHEEL_PADDLE_CLUTCH_ADJUSTING);
    assert(processor.paddle_adjustment_deadline_ms == 1000);
    assert(value.buttons == 0xf6);

    value.now_ms = 999;
    value.buttons = 1;
    process(&processor, &value, axes);
    assert(value.bite_point_percent == 50);
    assert(value.buttons == 0);
    assert(processor.overrides.axis_7.value == 128);
    uint8_t report_percent = 0;
    assert(!wheel_axis_override_take_bite_point_report(&processor, value.bite_point_percent,
                                                       &report_percent));

    value.now_ms = 1000;
    value.buttons = 1;
    process(&processor, &value, axes);
    assert(value.bite_point_percent == 51);
    assert(processor.paddle_adjustment_deadline_ms == 1800);
    assert(processor.overrides.axis_7.value == 125);
    assert(wheel_axis_override_take_bite_point_report(&processor, value.bite_point_percent,
                                                      &report_percent));
    assert(report_percent == 51);
    assert(!wheel_axis_override_take_bite_point_report(&processor, value.bite_point_percent,
                                                       &report_percent));

    value.now_ms = 1800;
    value.motion = -1;
    process(&processor, &value, axes);
    assert(value.bite_point_percent == 50);
    assert(value.motion == 0);
    assert(wheel_axis_override_take_bite_point_report(&processor, value.bite_point_percent,
                                                      &report_percent));
    assert(report_percent == 50);

    value.now_ms = 1801;
    process(&processor, &value, axes);
    assert(processor.paddle_adjustment_deadline_ms == 1801);
    value.now_ms = 1802;
    value.motion = 1;
    process(&processor, &value, axes);
    assert(value.bite_point_percent == 51);
    assert(wheel_axis_override_take_bite_point_report(&processor, value.bite_point_percent,
                                                      &report_percent));
    assert(report_percent == 51);

    value.now_ms = 1803;
    value.x = 11;
    value.motion = 0;
    process(&processor, &value, axes);
    assert(processor.paddle_clutch_phase == WHEEL_PADDLE_CLUTCH_IDLE);
    uint8_t updated_percent = 0;
    assert(wheel_axis_override_take_bite_point(&processor, value.bite_point_percent,
                                               &updated_percent));
    assert(updated_percent == 51);
    assert(!wheel_axis_override_take_bite_point(&processor, value.bite_point_percent,
                                                &updated_percent));
}

static void test_selects_axis_overrides(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    uint8_t axes[2] = {0x31, 0xc2};

    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED);
    process(&processor, &value, axes);
    assert(processor.overrides.axis_7.enabled);
    assert(processor.overrides.axis_7.value == 0x24);
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

static void test_applies_paddle_clutch_bite_point_sequence(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    TestInput value = input(WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED);
    uint8_t axes[2] = {0};

    value.x = 5;
    value.y = 5;
    process(&processor, &value, axes);
    assert(processor.paddle_clutch_phase == WHEEL_PADDLE_CLUTCH_ARMED);
    assert(processor.overrides.axis_7.value == 5);

    value.x = 5;
    value.y = 0xf5;
    process(&processor, &value, axes);
    assert(processor.paddle_clutch_phase == WHEEL_PADDLE_CLUTCH_ARMED);

    value.y = 0xf6;
    process(&processor, &value, axes);
    assert(processor.paddle_clutch_phase == WHEEL_PADDLE_CLUTCH_ACTIVE);
    assert(processor.overrides.axis_7.value == 5);

    process(&processor, &value, axes);
    assert(processor.overrides.axis_7.value == 130);

    value.x = UINT8_MAX;
    value.y = UINT8_MAX;
    process(&processor, &value, axes);
    assert(processor.overrides.axis_7.value == UINT8_MAX);
    assert(processor.paddle_clutch_phase == WHEEL_PADDLE_CLUTCH_IDLE);
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
    assert(processor.y_available);

    value.x = 0x88;
    value.y = UINT8_MAX;
    process(&processor, &value, axes);
    assert(!processor.x_available);
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

static void test_normalizes_crc_packet_axis_controls(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    uint8_t controls[8] = {0, 0, 0, 0, 0, 0x20, 0x40, 0};
    uint8_t axes[2] = {0x11, 0x22};

    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 6, 0, 0x17, 0, controls, axes);
    assert(controls[5] == 0xbf);
    assert(controls[6] == 0x7f);
    assert(axes[0] == 0x11);
    assert(axes[1] == 0x22);

    controls[5] = 0x20;
    controls[6] = 0x40;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 6, 0, 0x18, 0, controls, axes);
    assert(controls[5] == 0xdf);
    assert(controls[6] == 0xbf);

    controls[5] = 0x7f;
    controls[6] = 0x7e;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 6, 0, 0x17, 0, controls, axes);
    assert(controls[5] == 0);
    assert(controls[6] == 0x03);

    controls[5] = 0x20;
    controls[6] = 0x40;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 0x15, 0, 0, 0, controls, axes);
    assert(controls[5] == 0x20);
    assert(controls[6] == 0x40);
}

static void test_tracks_crc_packet_axis_report_availability(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    uint8_t controls[8] = {0};
    uint8_t axes[2] = {0};

    controls[7] = 1;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 6, 0, 0, 0, controls, axes);
    assert(processor.packet_axis_report_enabled);

    controls[7] = 0;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 6, 0, 0, 0, controls, axes);
    assert(processor.packet_axis_report_enabled);

    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 0x15, 0, 0, 0, controls, axes);
    assert(!processor.packet_axis_report_enabled);
}

static void test_publishes_crc_packet_overrides(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    uint8_t controls[8] = {0, 0, 0, 0, 0, 0x20, 0x40, 1};
    uint8_t axes[2] = {0x11, 0x22};

    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED, 0x15, 0, 0, 50, controls, axes);
    assert(processor.overrides.axis_7.enabled);
    assert(processor.overrides.axis_7.value == 0x20);
    assert(controls[5] == 0x80);
    assert(controls[6] == 0x80);

    controls[5] = 0x20;
    controls[6] = 0x40;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_SECONDARY, 0x15, 0, 0, 0, controls, axes);
    assert(processor.overrides.axis_7.enabled);
    assert(processor.overrides.axis_7.value == 0x20);
    assert(processor.overrides.auxiliary.enabled);
    assert(processor.overrides.auxiliary.value == 0x40);

    controls[5] = 0x20;
    controls[6] = 0x40;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_PRIMARY, 0x15, 0, 0, 0, controls, axes);
    assert(processor.overrides.axis_6.enabled);
    assert(processor.overrides.axis_6.value == 0x20);
    assert(processor.overrides.axis_5.enabled);
    assert(processor.overrides.axis_5.value == 0x40);
}

static void test_multiplexes_crc_packet_axes(void) {
    WheelAxisOverrideProcessor processor;
    wheel_axis_override_processor_init(&processor);
    uint8_t controls[8] = {0, 0, 0, 0, 0, 0x20, 0x40, 1};
    uint8_t axes[2] = {0x11, 0x22};

    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 6, 0, 0, controls, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_X);
    assert(axes[0] == 0x11);
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 6, 0, 0, controls, axes);
    assert(axes[0] == 0x10);

    controls[5] = UINT8_MAX;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 6, 0, 0, controls, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_SELECT);
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 6, 0, 0, controls, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_Y);
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 6, 0, 0, controls, axes);
    assert(axes[0] == 0xa1);

    wheel_axis_override_processor_init(&processor);
    controls[5] = UINT8_MAX;
    controls[6] = UINT8_MAX;
    axes[0] = 0;
    axes[1] = 0x22;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 7, 0, 0, controls, axes);
    assert(axes[0] == UINT8_MAX);
    assert(axes[1] == 0);

    wheel_axis_override_processor_init(&processor);
    controls[5] = UINT8_MAX;
    controls[6] = 0x40;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 8, 0, 0, controls, axes);
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 8, 0, 0, controls, axes);
    assert(axes[0] == 0xdf);

    wheel_axis_override_processor_init(&processor);
    controls[5] = 0x20;
    controls[6] = 0x40;
    process_packet(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0x15, 0, 0, 0, controls, axes);
    assert(axes[0] == 0xa0);
    assert(axes[1] == 0xc0);
    assert(controls[5] == 0xa0);
    assert(controls[6] == 0xc0);
}

static void test_publishes_axis_mode_overrides_and_inactive_outputs(void) {
    WheelAxisOverrideProcessor processor;
    uint8_t controls[8] = {0, 0, 0, 0, 0x24, 0x68, 0, 0};
    uint8_t axes[2] = {0x11, 0x22};
    wheel_axis_override_processor_init(&processor);

    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_SECONDARY, 0, controls, axes);
    assert(processor.overrides.axis_7.enabled);
    assert(processor.overrides.axis_7.value == 0x24);
    assert(processor.overrides.auxiliary.enabled);
    assert(processor.overrides.auxiliary.value == 0x68);
    assert(axes[0] == 0);
    assert(axes[1] == 0);

    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_PRIMARY, 7, controls, axes);
    assert(processor.overrides.axis_6.enabled);
    assert(processor.overrides.axis_6.value == 0x24);
    assert(processor.overrides.axis_5.enabled);
    assert(processor.overrides.axis_5.value == 0x68);
    assert(!processor.overrides.axis_7.enabled);
    assert(!processor.overrides.auxiliary.enabled);
    assert(axes[0] == UINT8_MAX);
    assert(axes[1] == 0);

    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_NONE, 8, controls, axes);
    assert(!processor.overrides.axis_5.enabled);
    assert(!processor.overrides.axis_6.enabled);
    assert(axes[0] == UINT8_MAX);
    assert(axes[1] == 0);
}

static void test_multiplexes_axis_mode_packet_axes(void) {
    WheelAxisOverrideProcessor processor;
    uint8_t controls[8] = {0, 0, 0, 0, 20, 40, 4, 0};
    uint8_t axes[2] = {0};
    wheel_axis_override_processor_init(&processor);

    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 0, controls, axes);
    assert(axes[0] == 0x94);
    assert(axes[1] == 0xa8);
    assert(processor.x_available);
    assert(processor.y_available);

    wheel_axis_override_processor_init(&processor);
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 6, controls, axes);
    assert(processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_X);
    assert(axes[0] == 0x80);
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 6, controls, axes);
    assert(axes[0] == 0x76);
    processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 6, controls, axes);
    assert(axes[0] == 0xeb);

    wheel_axis_override_processor_init(&processor);
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 7, controls, axes);
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 7, controls, axes);
    assert(axes[0] == 0x75);
    assert(axes[1] == 0);
    processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 7, controls, axes);
    assert(axes[0] == 0x94);

    wheel_axis_override_processor_init(&processor);
    processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_X;
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 8, controls, axes);
    assert(axes[0] == 10);
    processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 8, controls, axes);
    assert(axes[0] == 20);

    controls[4] = 0x88;
    controls[5] = UINT8_MAX;
    wheel_axis_override_processor_init(&processor);
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 7, controls, axes);
    assert(!processor.x_available);
    assert(!processor.y_available);
    assert(axes[0] == 0x80);

    controls[4] = UINT8_MAX;
    wheel_axis_override_processor_init(&processor);
    process_axis_mode(&processor, WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED, 7, controls, axes);
    assert(axes[0] == UINT8_MAX);
}

int main(void) {
    test_selects_axis_overrides();
    test_applies_paddle_clutch_bite_point_sequence();
    test_adjusts_and_publishes_paddle_clutch_bite_point();
    test_handles_disabled_and_fixed_axis_modes();
    test_maps_direct_multiplexed_axes();
    test_multiplexes_x_and_y_across_samples();
    test_applies_interface_specific_multiplex_encoding();
    test_normalizes_crc_packet_axis_controls();
    test_tracks_crc_packet_axis_report_availability();
    test_publishes_crc_packet_overrides();
    test_multiplexes_crc_packet_axes();
    test_publishes_axis_mode_overrides_and_inactive_outputs();
    test_multiplexes_axis_mode_packet_axes();
    return 0;
}
