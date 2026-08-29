#include "usb/host_capability_recovery.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_HOST_CAPABILITY_ARM_DELAY_MS = 300,
    USB_HOST_CAPABILITY_RETRY_DELAY_MS = 3000,
    USB_HOST_CAPABILITY_STANDARD = 0x0100,
    USB_HOST_CAPABILITY_UNIVERSAL = 0x0200,
    USB_HOST_CAPABILITY_MODE_SEVEN = 0x0800,
};

/**
 * @brief Initializes Xbox host-capability recovery.
 *
 * Clears the recovery deadline so the active operating mode can arm it on the first service pass.
 *
 * @param[out] recovery Recovery state to initialize.
 */
void usb_host_capability_recovery_init(UsbHostCapabilityRecovery *recovery) {
    *recovery = (UsbHostCapabilityRecovery){0};
}

/**
 * @brief Services Xbox host-capability recovery.
 *
 * Arms a 300-millisecond deadline outside the applicable Xbox wheel configurations. Applicable
 * configurations are modes 10, 18, or 28 with capability 0x0100, every mode with capability
 * 0x0200, mode 7 with capability 0x0800, or an attached adapter request. An enabled host
 * capability suppresses recovery without moving the deadline. A disabled capability requests USB
 * resume signaling after the deadline has strictly passed and then schedules a 3000-millisecond
 * retry.
 *
 * @param[in,out] recovery Persistent recovery deadline.
 * @param[in] input Current USB mode, wheel capability, adapter, and host state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Resume-signaling request when the disabled capability deadline has passed.
 */
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
    if (input.host_capability_enabled || (int32_t)(now_ms - recovery->deadline_ms) <= 0) {
        return USB_HOST_CAPABILITY_RECOVERY_NONE;
    }

    recovery->deadline_ms = now_ms + USB_HOST_CAPABILITY_RETRY_DELAY_MS;
    return USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME;
}
