#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "cooling/fan.h"

static void test_safe_startup(void) {
    FanController controller;

    fan_controller_init(&controller);
    assert(controller.duty_percent == 100);
    assert(fan_controller_update(&controller, 0, false, true) == 100);
    assert(fan_controller_update(&controller, 100, true, false) == 0);
}

static void test_temperature_levels(void) {
    FanController controller = {
        .level = FAN_LEVEL_OFF,
        .duty_percent = 0,
    };

    assert(fan_controller_update(&controller, 36, true, true) == 20);
    assert(fan_controller_update(&controller, 46, true, true) == 40);
    assert(fan_controller_update(&controller, 61, true, true) == 50);
    assert(fan_controller_update(&controller, 76, true, true) == 70);
    assert(fan_controller_update(&controller, 96, true, true) == 100);
}

static void test_hysteresis(void) {
    FanController controller = {
        .level = FAN_LEVEL_HIGH,
        .duty_percent = 50,
    };

    assert(fan_controller_update(&controller, 58, true, true) == 50);
    assert(fan_controller_update(&controller, 54, true, true) == 40);
    assert(fan_controller_update(&controller, 44, true, true) == 40);
    assert(fan_controller_update(&controller, 39, true, true) == 20);
    assert(fan_controller_update(&controller, 29, true, true) == 0);
}

static void test_tachometer(void) {
    assert(fan_tachometer_rpm(1000, 601000, 60000000, 2) == 3000);
    assert(fan_tachometer_rpm(UINT32_MAX - 99, 500, 60000000, 2) == UINT16_MAX);
    assert(fan_tachometer_rpm(0, 1, 60000000, 2) == UINT16_MAX);
    assert(fan_tachometer_rpm(0, 0, 60000000, 2) == 0);
    assert(fan_tachometer_rpm(0, 100, 0, 2) == 0);
    assert(fan_tachometer_rpm(0, 100, 60000000, 0) == 0);
}

int main(void) {
    test_safe_startup();
    test_temperature_levels();
    test_hysteresis();
    test_tachometer();
    return 0;
}
