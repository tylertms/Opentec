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

    assert(wheel_startup_adapter_version_page_build(&motor, &adapter, &page));
    expect_line(&page.lines[0], "BASE: 3.9.0");
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

    assert(wheel_startup_adapter_version_page_build(&motor, &adapter, &page));
    expect_line(&page.lines[1], "MOTOR: NA");
    expect_line(&page.lines[2], "ST WHEEL: 5.0.0");
    assert(!wheel_startup_adapter_version_page_build(&motor, NULL, &page));
}

int main(void) {
    test_builds_managed_motor_and_adapter_versions();
    test_reports_an_unavailable_legacy_motor();
    return 0;
}
