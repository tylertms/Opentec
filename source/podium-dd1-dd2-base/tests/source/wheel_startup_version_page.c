#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/accessory.h"
#include "wheel/adapter.h"
#include "wheel/startup_version_page.h"

static void expect_line(const WheelStartupVersionLine *line, const char *text) {
    uint8_t length = (uint8_t)strlen(text);
    assert(line->length == length);
    assert(memcmp(line->text, text, length) == 0);
}

static void test_builds_supported_auxiliary_and_adapter_versions(void) {
    const WheelAccessory standard = {
        .version = UINT32_C(0x000f0bca),
        .kind = WHEEL_ACCESSORY_STANDARD,
    };
    const WheelAccessory extended = {
        .version = UINT32_C(0x000f0bca),
        .kind = WHEEL_ACCESSORY_EXTENDED,
    };
    const WheelAdapterInput adapter = {
        .firmware_version = {5, 6, 107},
        .mode = 1,
        .connected = true,
    };
    WheelStartupVersionPage page;

    const uint16_t wheel_axis_values[2] = {0};

    assert(
        wheel_startup_adapter_version_page_build(&standard, &adapter, 1, wheel_axis_values, &page));
    expect_line(&page.lines[0], "BASE: 3.9.1");
    expect_line(&page.lines[1], "MOTOR: 10.11.15");
    expect_line(&page.lines[2], "ST WHEEL: 5.6.107");
    expect_line(&page.lines[3], " ");
    assert(
        wheel_startup_adapter_version_page_build(&extended, &adapter, 1, wheel_axis_values, &page));
    expect_line(&page.lines[1], "MOTOR: 10.11.15");
}

static void test_reports_unavailable_auxiliary_states_as_na(void) {
    const WheelAccessory legacy = {
        .version = UINT32_C(0x000f0bca),
        .kind = WHEEL_ACCESSORY_LEGACY,
    };
    const WheelAccessory disconnected = {
        .version = UINT32_C(0x000f0bca),
        .kind = WHEEL_ACCESSORY_DISCONNECTED,
    };
    const WheelAdapterInput adapter = {
        .firmware_version = {0xc5, 0, 0},
        .mode = 1,
        .connected = true,
    };
    WheelStartupVersionPage page;

    const uint16_t wheel_axis_values[2] = {0};

    assert(
        wheel_startup_adapter_version_page_build(&legacy, &adapter, 1, wheel_axis_values, &page));
    expect_line(&page.lines[1], "MOTOR: NA");
    expect_line(&page.lines[2], "ST WHEEL: 5.0.0");
    assert(wheel_startup_adapter_version_page_build(&disconnected, &adapter, 1, wheel_axis_values,
                                                    &page));
    expect_line(&page.lines[1], "MOTOR: NA");
    assert(wheel_startup_adapter_version_page_build(NULL, &adapter, 1, wheel_axis_values, &page));
    expect_line(&page.lines[1], "MOTOR: NA");
    assert(!wheel_startup_adapter_version_page_build(&legacy, NULL, 1, wheel_axis_values, &page));
}

static void test_uses_packed_axis_values_only_for_pulse_and_extended_modes(void) {
    const WheelAccessory accessory = {
        .version = UINT32_C(0x000f0bca),
        .kind = WHEEL_ACCESSORY_STANDARD,
    };
    const WheelAdapterInput adapter = {
        .firmware_version = {5, 6, 107},
        .mode = 1,
        .connected = true,
    };
    const uint16_t wheel_axis_values[2] = {0x0bca, 0xff0f};
    WheelStartupVersionPage page;

    assert(wheel_startup_adapter_version_page_build(&accessory, &adapter, 0x1a, wheel_axis_values,
                                                    &page));
    expect_line(&page.lines[2], "ST WHEEL: 5.6.107");
    assert(wheel_startup_adapter_version_page_build(&accessory, &adapter, 0x1b, wheel_axis_values,
                                                    &page));
    expect_line(&page.lines[2], "ST WHEEL: 10.11.15");
    assert(wheel_startup_adapter_version_page_build(&accessory, &adapter, 0x1c, wheel_axis_values,
                                                    &page));
    expect_line(&page.lines[2], "ST WHEEL: 10.11.15");
    assert(wheel_startup_adapter_version_page_build(&accessory, &adapter, 0x1d, wheel_axis_values,
                                                    &page));
    expect_line(&page.lines[2], "ST WHEEL: 5.6.107");
    assert(!wheel_startup_adapter_version_page_build(&accessory, &adapter, 0x1b, NULL, &page));

    const uint16_t maximum_axis_values[2] = {UINT16_MAX, UINT16_MAX};
    assert(wheel_startup_adapter_version_page_build(&accessory, &adapter, 0x1c, maximum_axis_values,
                                                    &page));
    expect_line(&page.lines[2], "ST WHEEL: 63.255.255");
}

int main(void) {
    test_builds_supported_auxiliary_and_adapter_versions();
    test_reports_unavailable_auxiliary_states_as_na();
    test_uses_packed_axis_values_only_for_pulse_and_extended_modes();
    return 0;
}
