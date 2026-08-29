#ifndef OPENTEC_BASE_USB_TUNING_PROFILE_SERVICE_H
#define OPENTEC_BASE_USB_TUNING_PROFILE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/vendor_command.h"

enum {
    USB_TUNING_PROFILE_RESET_DELAY_MS = 10000,
    USB_TUNING_PROFILE_MODE_DELAY_MS = 2000,
};

typedef enum {
    USB_TUNING_PROFILE_ACTION_NONE = 0,
    USB_TUNING_PROFILE_ACTION_CLAIM = 1 << 0,
    USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED = 1 << 1,
    USB_TUNING_PROFILE_ACTION_SAVE = 1 << 2,
    USB_TUNING_PROFILE_ACTION_MODE_CHANGED = 1 << 3,
    USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED = 1 << 4,
    USB_TUNING_PROFILE_ACTION_RESET_COMPLETED = 1 << 5,
    USB_TUNING_PROFILE_ACTION_MODE_TOGGLED = 1 << 6,
} UsbTuningProfileAction;

typedef struct {
    uint32_t reset_after_ms;
    uint32_t mode_change_after_ms;
    bool response_pending;
} UsbTuningProfileService;

void usb_tuning_profile_service_init(UsbTuningProfileService *service);
UsbTuningProfileAction usb_tuning_profile_service_apply(UsbTuningProfileService *service,
                                                        TuningProfileBank *bank,
                                                        const UsbVendorCommand *command,
                                                        uint32_t now_ms);
bool usb_tuning_profile_service_response_pending(const UsbTuningProfileService *service);
void usb_tuning_profile_service_response_sent(UsbTuningProfileService *service);

#endif
