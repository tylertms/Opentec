#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "remote_tuning/telemetry.h"
#include "usb/remote_tuning_records.h"

enum {
    USB_REMOTE_TUNING_SESSION_TIMEOUT_MS = 60000,
    USB_REMOTE_TUNING_HOST_REPORT_SIZE = 64,
};

/** @brief Host transport framing used for telemetry subscription reports. */
typedef enum {
    USB_REMOTE_TUNING_HOST_NATIVE,
    USB_REMOTE_TUNING_HOST_PLAYSTATION,
    USB_REMOTE_TUNING_HOST_XBOX,
} UsbRemoteTuningHost;

/** @brief Host remote-tuning session and retained downstream work. */
typedef struct {
    UsbRemoteTuningRecords records;
    RemoteTelemetry telemetry;
    uint32_t session_deadline_ms;
    uint16_t encoder_counter;
    uint8_t command_type;
    uint8_t setup_selection;
    uint8_t menu_selection;
    uint8_t multi_position_selection;
    uint8_t setup_index;
    uint8_t setup_page;
    uint8_t encoder_selection;
    int8_t physical_previous_input;
    uint8_t physical_button_flags;
    uint8_t physical_rotary_position;
    uint8_t physical_navigation_input;
    RemoteTuningResponse pending_response;
    bool active;
    bool refresh_requested;
    bool adapter_refresh_state;
    bool active_sync_pending;
    bool setup_sync_pending;
    bool refresh_sync_pending;
    bool physical_input_released;
    bool physical_rotary_initialized;
} UsbRemoteTuningService;

void usb_remote_tuning_service_init(UsbRemoteTuningService *service);
bool usb_remote_tuning_service_apply(UsbRemoteTuningService *service,
                                     const UsbVendorCommand *command, uint32_t now_ms,
                                     uint8_t wheel_mode, bool setup_selection_allowed,
                                     bool adapter_connected);
bool usb_remote_tuning_service_take_response(UsbRemoteTuningService *service, uint8_t wheel_mode,
                                             RemoteTuningResponse *response);
bool usb_remote_tuning_service_take_adapter_active(UsbRemoteTuningService *service,
                                                   bool synchronization_allowed, bool *active);
bool usb_remote_tuning_service_take_adapter_refresh_state(UsbRemoteTuningService *service,
                                                          bool *active);
bool usb_remote_tuning_service_take_adapter_setup_selection(UsbRemoteTuningService *service,
                                                            uint8_t *selection);
uint8_t
usb_remote_tuning_service_queue_host_controls(UsbRemoteTuningService *service,
                                              const uint8_t input[REMOTE_TELEMETRY_REPORT_SIZE]);
bool usb_remote_tuning_service_take_forward_batch(
    UsbRemoteTuningService *service, uint8_t wheel_mode,
    uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE], uint8_t *length);
bool usb_remote_tuning_service_take_host_report(UsbRemoteTuningService *service, uint8_t wheel_mode,
                                                UsbRemoteTuningHost host,
                                                uint8_t output[USB_REMOTE_TUNING_HOST_REPORT_SIZE]);
bool usb_remote_tuning_service_take_telemetry_report(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode,
                                                     uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]);
bool usb_remote_tuning_service_update_physical_selection(UsbRemoteTuningService *service,
                                                         uint8_t wheel_mode, bool profile_mode,
                                                         bool tuning_display_supported,
                                                         bool adapter_connected,
                                                         int8_t tuning_input,
                                                         uint8_t auxiliary_buttons);
bool usb_remote_tuning_service_update_legacy_encoder(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode, uint8_t rotary_position);
bool usb_remote_tuning_service_update_setup_navigation(UsbRemoteTuningService *service,
                                                       uint8_t wheel_mode, bool profile_mode,
                                                       uint8_t motion);

#endif
