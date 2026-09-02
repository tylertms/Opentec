#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb/host_capability_recovery.h"

static UsbHostCapabilityRecoveryInput input(uint8_t wheel_mode, uint16_t capability_flags) {
    return (UsbHostCapabilityRecoveryInput){
        .wheel_mode = wheel_mode,
        .wheel_capability_flags = capability_flags,
        .xbox_mode = true,
    };
}

static void test_arms_deadline_outside_applicable_xbox_modes(void) {
    UsbHostCapabilityRecovery recovery;
    usb_host_capability_recovery_init(&recovery);
    UsbHostCapabilityRecoveryInput state = input(10, 0x0100);

    state.xbox_mode = false;
    assert(usb_host_capability_recovery_update(&recovery, state, 100) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(recovery.deadline_ms == 400);

    state.xbox_mode = true;
    state.wheel_mode = 9;
    assert(usb_host_capability_recovery_update(&recovery, state, 200) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(recovery.deadline_ms == 500);
}

static void test_selects_every_applicable_wheel_configuration(void) {
    const UsbHostCapabilityRecoveryInput states[] = {
        input(10, 0x0100),
        input(18, 0x0100),
        input(28, 0x0100),
        input(3, 0x0200),
        input(7, 0x0800),
        {
            .xbox_mode = true,
            .adapter_requests_capability = true,
        },
    };

    for (uint8_t index = 0; index < sizeof(states) / sizeof(states[0]); index++) {
        UsbHostCapabilityRecovery recovery = {.deadline_ms = 100};
        assert(usb_host_capability_recovery_update(&recovery, states[index], 101) ==
               USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME);
        assert(recovery.deadline_ms == 3101);
    }
}

static void test_rejects_nearby_wheel_configurations(void) {
    const UsbHostCapabilityRecoveryInput states[] = {
        input(9, 0x0100), input(10, 0x0800), input(18, 0x0800), input(28, 0x0800), input(7, 0x0100),
    };

    for (uint8_t index = 0; index < sizeof(states) / sizeof(states[0]); index++) {
        UsbHostCapabilityRecovery recovery = {.deadline_ms = 100};
        assert(usb_host_capability_recovery_update(&recovery, states[index], 101) ==
               USB_HOST_CAPABILITY_RECOVERY_NONE);
        assert(recovery.deadline_ms == 401);
    }
}

static void test_requires_deadline_to_be_strictly_passed(void) {
    UsbHostCapabilityRecovery recovery = {.deadline_ms = 400};
    UsbHostCapabilityRecoveryInput state = input(10, 0x0100);

    assert(usb_host_capability_recovery_update(&recovery, state, 400) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(recovery.deadline_ms == 400);
    assert(usb_host_capability_recovery_update(&recovery, state, 401) ==
           USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME);
    assert(recovery.deadline_ms == 3401);
    assert(usb_host_capability_recovery_update(&recovery, state, 3401) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(usb_host_capability_recovery_update(&recovery, state, 3402) ==
           USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME);
}

static void test_enabled_capability_rearms_deadline(void) {
    UsbHostCapabilityRecovery recovery = {.deadline_ms = 400};
    UsbHostCapabilityRecoveryInput state = input(10, 0x0100);
    state.host_capability_enabled = true;

    assert(usb_host_capability_recovery_update(&recovery, state, 500) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(recovery.deadline_ms == 800);

    state.host_capability_enabled = false;
    assert(usb_host_capability_recovery_update(&recovery, state, 800) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(usb_host_capability_recovery_update(&recovery, state, 801) ==
           USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME);
    assert(recovery.deadline_ms == 3801);

    state.host_capability_enabled = true;
    assert(usb_host_capability_recovery_update(&recovery, state, 900) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(recovery.deadline_ms == 1200);
}

static void test_deadline_survives_counter_wrap(void) {
    UsbHostCapabilityRecovery recovery = {.deadline_ms = UINT32_MAX};
    UsbHostCapabilityRecoveryInput state = input(10, 0x0100);

    assert(usb_host_capability_recovery_update(&recovery, state, UINT32_MAX) ==
           USB_HOST_CAPABILITY_RECOVERY_NONE);
    assert(usb_host_capability_recovery_update(&recovery, state, 0) ==
           USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME);
    assert(recovery.deadline_ms == 3000);
}

int main(void) {
    test_arms_deadline_outside_applicable_xbox_modes();
    test_selects_every_applicable_wheel_configuration();
    test_rejects_nearby_wheel_configurations();
    test_requires_deadline_to_be_strictly_passed();
    test_enabled_capability_rearms_deadline();
    test_deadline_survives_counter_wrap();
    return 0;
}
