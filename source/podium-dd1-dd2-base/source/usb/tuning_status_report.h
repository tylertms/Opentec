#ifndef OPENTEC_BASE_USB_TUNING_STATUS_REPORT_H
#define OPENTEC_BASE_USB_TUNING_STATUS_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

typedef struct {
    uint16_t base_status;
    uint8_t hardware_mode;
    uint8_t auxiliary_mode;
    uint8_t auxiliary_flags;
    uint32_t auxiliary_status;
    uint8_t wheel_status_low;
    uint8_t wheel_status_high;
    uint8_t wheel_mode;
    uint8_t button_mode;
    uint32_t input;
    uint8_t adapter_mode;
    uint8_t adapter[5];
    uint8_t pedal_status;
    uint8_t pedal_auxiliary;
    uint8_t pedal_axis_low;
    uint8_t pedal_axis_high;
    bool tuning_available;
    bool system_active;
    uint8_t force_effect;
    uint8_t system_flags;
    uint8_t output_status;
    bool interface_gate;
} UsbTuningStatusSnapshot;

typedef struct {
    uint8_t previous[USB_DEVICE_REPORT_SIZE];
    bool enabled;
    bool dirty;
    bool previous_valid;
} UsbTuningStatusReportService;

void usb_tuning_status_report_service_init(UsbTuningStatusReportService *service);
bool usb_tuning_status_report_apply_command(UsbTuningStatusReportService *service,
                                            const UsbVendorCommand *command);
bool usb_tuning_status_report_prepare(const UsbTuningStatusReportService *service,
                                      const UsbTuningStatusSnapshot *snapshot,
                                      uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_tuning_status_report_commit(UsbTuningStatusReportService *service,
                                     const uint8_t report[USB_DEVICE_REPORT_SIZE]);

#endif
