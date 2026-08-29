#ifndef OPENTEC_BASE_USB_HOST_CAPABILITY_RECOVERY_H
#define OPENTEC_BASE_USB_HOST_CAPABILITY_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t wheel_mode;
    uint16_t wheel_capability_flags;
    bool xbox_mode;
    bool host_capability_enabled;
    bool adapter_requests_capability;
} UsbHostCapabilityRecoveryInput;

typedef struct {
    uint32_t deadline_ms;
} UsbHostCapabilityRecovery;

typedef enum {
    USB_HOST_CAPABILITY_RECOVERY_NONE,
    USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME,
} UsbHostCapabilityRecoveryAction;

void usb_host_capability_recovery_init(UsbHostCapabilityRecovery *recovery);
UsbHostCapabilityRecoveryAction
usb_host_capability_recovery_update(UsbHostCapabilityRecovery *recovery,
                                    UsbHostCapabilityRecoveryInput input, uint32_t now_ms);

#endif
