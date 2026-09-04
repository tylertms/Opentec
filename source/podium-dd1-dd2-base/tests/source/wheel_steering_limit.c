#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"
#include "wheel/steering_limit.h"

static UsbOperatingModeCommand command_source(uint8_t selector, uint8_t operation,
                                              uint8_t percent) {
    UsbOperatingModeCommand source = {.opcode = 1};
    source.parameters[0] = selector;
    source.parameters[1] = operation;
    source.parameters[2] = percent;
    return source;
}

static void test_defaults_cover_all_profiles(void) {
    WheelSteeringLimits limits = {0};
    wheel_steering_limits_defaults(&limits);
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        assert(limits.percent[profile] == 100);
    }
}

static void test_decodes_set_and_reset_commands(void) {
    WheelSteeringLimitCommand command;
    UsbOperatingModeCommand source = command_source(0x17, 1, 65);
    assert(wheel_steering_limit_command_decode(&source, &command));
    assert(command.percent == 65);
    assert(!command.reset_all);

    source.parameters[2] = 101;
    assert(wheel_steering_limit_command_decode(&source, &command));
    assert(command.reset_all);
}

static void test_rejects_other_device_control_commands(void) {
    WheelSteeringLimitCommand command;
    UsbOperatingModeCommand source = command_source(0x16, 1, 50);
    assert(!wheel_steering_limit_command_decode(&source, &command));
    source.parameters[0] = 0x17;
    source.parameters[1] = 0;
    assert(!wheel_steering_limit_command_decode(&source, &command));
    source.parameters[1] = 1;
    source.opcode = 0;
    assert(!wheel_steering_limit_command_decode(&source, &command));
    assert(!wheel_steering_limit_command_decode(NULL, &command));
    assert(!wheel_steering_limit_command_decode(&source, NULL));
}

static void test_active_limit_rejects_invalid_inputs(void) {
    WheelSteeringLimits limits;
    wheel_steering_limits_defaults(&limits);
    assert(wheel_steering_limits_active(NULL, 0) == 100);
    assert(wheel_steering_limits_active(&limits, TUNING_PROFILE_SLOT_COUNT) == 100);
}

int main(void) {
    test_defaults_cover_all_profiles();
    test_decodes_set_and_reset_commands();
    test_rejects_other_device_control_commands();
    test_active_limit_rejects_invalid_inputs();
    return 0;
}
