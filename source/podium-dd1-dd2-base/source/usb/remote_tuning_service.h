#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/remote_tuning_records.h"

enum {
    USB_REMOTE_TUNING_SESSION_TIMEOUT_MS = 60000,
    USB_REMOTE_TUNING_RESPONSE_NONE = 0,
    USB_REMOTE_TUNING_RESPONSE_ACTIVE = 2,
    USB_REMOTE_TUNING_RESPONSE_SETUP = 4,
    USB_REMOTE_TUNING_RESPONSE_REFRESH = 5,
    USB_REMOTE_TUNING_RESPONSE_INACTIVE = 0xff,
};

/** @brief Attached-wheel transport selected for a pending remote-tuning response. */
typedef enum {
    USB_REMOTE_TUNING_RESPONSE_TARGET_NONE,
    USB_REMOTE_TUNING_RESPONSE_TARGET_LEGACY,
    USB_REMOTE_TUNING_RESPONSE_TARGET_EXTENDED,
} UsbRemoteTuningResponseTarget;

/** @brief Host remote-tuning session and retained downstream work. */
typedef struct {
    UsbRemoteTuningRecords records;
    uint32_t session_deadline_ms;
    uint16_t encoder_counter;
    uint8_t command_type;
    uint8_t setup_selection;
    uint8_t menu_selection;
    uint8_t multi_position_selection;
    uint8_t setup_index;
    uint8_t encoder_selection;
    uint8_t pending_response;
    UsbRemoteTuningResponseTarget response_target;
    bool active;
    bool refresh_requested;
    bool vendor_response_pending;
    bool active_sync_pending;
    bool setup_sync_pending;
    bool refresh_sync_pending;
} UsbRemoteTuningService;

void usb_remote_tuning_service_init(UsbRemoteTuningService *service);
bool usb_remote_tuning_service_apply(UsbRemoteTuningService *service,
                                     const UsbVendorCommand *command, uint32_t now_ms,
                                     uint8_t wheel_mode, bool setup_selection_allowed,
                                     bool adapter_connected);

#endif
