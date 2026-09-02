#include "usb/host_capability_recovery.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal delays and capability bits used by host-capability recovery. */
enum {
    USB_HOST_CAPABILITY_ARM_DELAY_MS =
        300, /**< Delay before recovery is first due, in milliseconds. */
    USB_HOST_CAPABILITY_RETRY_DELAY_MS =
        3000,                               /**< Delay before retrying recovery, in milliseconds. */
    USB_HOST_CAPABILITY_STANDARD = 0x0100,  /**< Capability bit for standard wheel modes. */
    USB_HOST_CAPABILITY_UNIVERSAL = 0x0200, /**< Capability bit valid for every wheel mode. */
    USB_HOST_CAPABILITY_MODE_SEVEN = 0x0800, /**< Capability bit for wheel mode seven. */
};

void usb_host_capability_recovery_init(UsbHostCapabilityRecovery *recovery) {
    *recovery = (UsbHostCapabilityRecovery){0};
}

UsbHostCapabilityRecoveryAction
usb_host_capability_recovery_update(UsbHostCapabilityRecovery *recovery,
                                    UsbHostCapabilityRecoveryInput input, uint32_t now_ms) {
    bool standard_mode = input.wheel_mode == 10 || input.wheel_mode == 18 || input.wheel_mode == 28;
    bool required =
        (standard_mode && (input.wheel_capability_flags & USB_HOST_CAPABILITY_STANDARD) != 0) ||
        (input.wheel_capability_flags & USB_HOST_CAPABILITY_UNIVERSAL) != 0 ||
        (input.wheel_mode == 7 &&
         (input.wheel_capability_flags & USB_HOST_CAPABILITY_MODE_SEVEN) != 0) ||
        input.adapter_requests_capability;

    if (!input.xbox_mode || !required) {
        recovery->deadline_ms = now_ms + USB_HOST_CAPABILITY_ARM_DELAY_MS;
        return USB_HOST_CAPABILITY_RECOVERY_NONE;
    }
    if (input.host_capability_enabled) {
        recovery->deadline_ms = now_ms + USB_HOST_CAPABILITY_ARM_DELAY_MS;
        return USB_HOST_CAPABILITY_RECOVERY_NONE;
    }
    if ((int32_t)(now_ms - recovery->deadline_ms) <= 0) {
        return USB_HOST_CAPABILITY_RECOVERY_NONE;
    }

    recovery->deadline_ms = now_ms + USB_HOST_CAPABILITY_RETRY_DELAY_MS;
    return USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME;
}
