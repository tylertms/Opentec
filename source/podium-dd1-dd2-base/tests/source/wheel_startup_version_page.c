#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/identity.h"
#include "wheel/adapter.h"
#include "wheel/startup_version_page.h"

static void expect_line(const WheelStartupVersionLine *line, const char *text) {
    uint8_t length = (uint8_t)strlen(text);
    assert(line->length == length);
    assert(memcmp(line->text, text, length) == 0);
}

static void test_builds_managed_motor_and_adapter_versions(void) {
    const MotorIdentity motor = {
        .protocol = MOTOR_PROTOCOL_POSITION,
        .version = {1, 12, 203, 4},
    };
    const WheelAdapterInput adapter = {
        .firmware_version = {5, 6, 107},
        .mode = 1,
        .connected = true,
    };
    WheelStartupVersionPage page;

    const uint16_t wheel_axis_values[2] = {0};

    assert(wheel_startup_adapter_version_page_build(&motor, &adapter, 1, wheel_axis_values, &page));
    expect_line(&page.lines[0], "BASE: 3.9.1");
    expect_line(&page.lines[1], "MOTOR: 1.12.203");
    expect_line(&page.lines[2], "ST WHEEL: 5.6.107");
    expect_line(&page.lines[3], " ");
}

static void test_reports_an_unavailable_legacy_motor(void) {
    const MotorIdentity motor = {.protocol = MOTOR_PROTOCOL_LEGACY};
    const WheelAdapterInput adapter = {
        .firmware_version = {0xc5, 0, 0},
        .mode = 1,
        .connected = true,
    };
    WheelStartupVersionPage page;

    const uint16_t wheel_axis_values[2] = {0};

    assert(wheel_startup_adapter_version_page_build(&motor, &adapter, 1, wheel_axis_values, &page));
    expect_line(&page.lines[1], "MOTOR: NA");
    expect_line(&page.lines[2], "ST WHEEL: 5.0.0");
    assert(!wheel_startup_adapter_version_page_build(&motor, NULL, 1, wheel_axis_values, &page));
}

static void test_uses_packed_axis_values_only_for_pulse_and_extended_modes(void) {
    const MotorIdentity motor = {.protocol = MOTOR_PROTOCOL_POSITION};
    const WheelAdapterInput adapter = {
        .firmware_version = {5, 6, 107},
        .mode = 1,
        .connected = true,
    };
    const uint16_t wheel_axis_values[2] = {0x0bca, 0xff0f};
    WheelStartupVersionPage page;

    assert(
        wheel_startup_adapter_version_page_build(&motor, &adapter, 0x1a, wheel_axis_values, &page));
    expect_line(&page.lines[2], "ST WHEEL: 5.6.107");
    assert(
        wheel_startup_adapter_version_page_build(&motor, &adapter, 0x1b, wheel_axis_values, &page));
    expect_line(&page.lines[2], "ST WHEEL: 10.11.15");
    assert(
        wheel_startup_adapter_version_page_build(&motor, &adapter, 0x1c, wheel_axis_values, &page));
    expect_line(&page.lines[2], "ST WHEEL: 10.11.15");
    assert(
        wheel_startup_adapter_version_page_build(&motor, &adapter, 0x1d, wheel_axis_values, &page));
    expect_line(&page.lines[2], "ST WHEEL: 5.6.107");
    assert(!wheel_startup_adapter_version_page_build(&motor, &adapter, 0x1b, NULL, &page));

    const uint16_t maximum_axis_values[2] = {UINT16_MAX, UINT16_MAX};
    assert(wheel_startup_adapter_version_page_build(&motor, &adapter, 0x1c, maximum_axis_values,
                                                    &page));
    expect_line(&page.lines[2], "ST WHEEL: 63.255.255");
}

int main(void) {
    test_builds_managed_motor_and_adapter_versions();
    test_reports_an_unavailable_legacy_motor();
    test_uses_packed_axis_values_only_for_pulse_and_extended_modes();
    return 0;
}
