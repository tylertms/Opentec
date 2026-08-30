#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/display_output.h"
#include "wheel/startup_display.h"

static void test_waits_for_an_active_wheel(void) {
    WheelStartupDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 2, 3}};
    wheel_startup_display_init(&display);

    assert(!wheel_startup_display_update(&display, false, false, true, 0, 100, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_DASHES);
    assert(output.glyphs[0] == 1);
    assert(!wheel_startup_display_ready(&display));
}

static void test_presents_base_version_and_ready_delay(void) {
    WheelStartupDisplay display;
    WheelDisplayOutput output = {.third_glyph_marker = true};
    wheel_startup_display_init(&display);

    assert(wheel_startup_display_update(&display, true, false, true, 0, 1, &output));
    assert(output.glyphs[0] == 0x40);
    assert(output.glyphs[1] == 0x40);
    assert(output.glyphs[2] == 0x40);
    assert(!output.third_glyph_marker);
    assert(display.phase == WHEEL_STARTUP_DISPLAY_BASE_VERSION);
    assert(display.deadline_ms == 1001);

    assert(wheel_startup_display_update(&display, true, false, true, 0, 2, &output));
    assert(output.glyphs[0] == 0xcf);
    assert(output.glyphs[1] == 0xef);
    assert(output.glyphs[2] == 0x3f);
    assert(!wheel_startup_display_update(&display, true, false, true, 0, 1000, &output));
    assert(!wheel_startup_display_update(&display, true, false, true, 0, 1001, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_READY_DELAY);
    assert(display.deadline_ms == 2001);

    assert(wheel_startup_display_update(&display, true, false, true, 0, 1001, &output));
    assert(output.glyphs[0] == 0x40);
    assert(!wheel_startup_display_update(&display, true, false, true, 0, 2001, &output));
    assert(wheel_startup_display_update(&display, true, false, true, 0, 2002, &output));
    assert(output.glyphs[0] == 0);
    assert(output.glyphs[1] == 0);
    assert(output.glyphs[2] == 0);
    assert(wheel_startup_display_ready(&display));
}

static void test_presents_managed_motor_version(void) {
    WheelStartupDisplay display;
    WheelDisplayOutput output = {0};
    MotorIdentity motor = {
        .protocol = MOTOR_PROTOCOL_STANDARD,
        .version = {1, 2, 3, 4},
    };
    wheel_startup_display_init(&display);

    assert(wheel_startup_display_update(&display, true, false, true, &motor, 1, &output));
    assert(wheel_startup_display_update(&display, true, false, true, &motor, 1001, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_MOTOR_VERSION);
    assert(display.deadline_ms == 2001);
    assert(wheel_startup_display_update(&display, true, false, true, &motor, 1002, &output));
    assert(output.glyphs[0] == 0x86);
    assert(output.glyphs[1] == 0xdb);
    assert(output.glyphs[2] == 0x4f);
    assert(!wheel_startup_display_update(&display, true, false, true, &motor, 2001, &output));
    assert(!wheel_startup_display_update(&display, true, false, true, &motor, 2002, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_READY_DELAY);
    assert(display.deadline_ms == 3002);
}

static void test_reserves_longer_version_time_for_tuning_displays(void) {
    WheelStartupDisplay display;
    WheelDisplayOutput output = {0};
    wheel_startup_display_init(&display);

    assert(wheel_startup_display_update(&display, true, true, true, 0, 1, &output));
    assert(display.deadline_ms == 3001);
    assert(wheel_startup_display_take_version_presentation(&display));
    assert(!wheel_startup_display_take_version_presentation(&display));
    assert(!wheel_startup_display_update(&display, true, true, true, 0, 2, &output));
    assert(output.glyphs[0] == 0x40);
    assert(!wheel_startup_display_update(&display, true, true, true, 0, 3000, &output));
    assert(!wheel_startup_display_update(&display, true, true, true, 0, 3001, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_READY_DELAY);
    assert(display.deadline_ms == 4001);
    assert(!wheel_startup_display_update(&display, true, true, true, 0, 3501, &output));
    assert(!wheel_startup_display_take_version_presentation_close(&display));
    assert(!wheel_startup_display_update(&display, true, true, true, 0, 3502, &output));
    assert(wheel_startup_display_take_version_presentation_close(&display));
    assert(!wheel_startup_display_take_version_presentation_close(&display));
}

static void test_blinks_calibration_until_position_is_ready(void) {
    WheelStartupDisplay display;
    WheelDisplayOutput output = {0};
    wheel_startup_display_init(&display);

    assert(wheel_startup_display_update(&display, true, false, false, 0, 1, &output));
    assert(wheel_startup_display_update(&display, true, false, false, 0, 1001, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_CALIBRATION);
    assert(display.deadline_ms == 2001);

    assert(wheel_startup_display_update(&display, true, false, false, 0, 1002, &output));
    assert(output.glyphs[0] == 0x39);
    assert(output.glyphs[1] == 0x77);
    assert(output.glyphs[2] == 0x38);
    assert(!wheel_startup_display_update(&display, true, false, false, 0, 2001, &output));
    assert(wheel_startup_display_update(&display, true, false, false, 0, 2002, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_CALIBRATION_PAUSE);
    assert(display.deadline_ms == 2502);
    assert(output.glyphs[0] == 0);

    assert(!wheel_startup_display_update(&display, true, false, false, 0, 2502, &output));
    assert(!wheel_startup_display_update(&display, true, false, false, 0, 2503, &output));
    assert(display.phase == WHEEL_STARTUP_DISPLAY_CALIBRATION);
    assert(display.deadline_ms == 3003);
    assert(wheel_startup_display_update(&display, true, false, false, 0, 2504, &output));
    assert(wheel_startup_display_update(&display, true, false, true, 0, 3004, &output));
    assert(wheel_startup_display_ready(&display));
    assert(output.glyphs[0] == 0);
}

int main(void) {
    test_waits_for_an_active_wheel();
    test_presents_base_version_and_ready_delay();
    test_presents_managed_motor_version();
    test_reserves_longer_version_time_for_tuning_displays();
    test_blinks_calibration_until_position_is_ready();
    return 0;
}
