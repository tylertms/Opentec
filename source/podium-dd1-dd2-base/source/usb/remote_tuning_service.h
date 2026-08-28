#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "usb/remote_tuning_records.h"

enum {
    USB_REMOTE_TUNING_SESSION_TIMEOUT_MS = 60000,
};

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
    uint8_t setup_page;
    uint8_t encoder_selection;
    RemoteTuningResponse pending_response;
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
bool usb_remote_tuning_service_take_response(UsbRemoteTuningService *service, uint8_t wheel_mode,
                                             RemoteTuningResponse *response);

#endif
