#ifndef OPENTEC_BASE_USB_DIAGNOSTIC_REPORT_H
#define OPENTEC_BASE_USB_DIAGNOSTIC_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"
#include "wheel/status_service.h"

typedef struct {
    uint8_t version;
    int8_t initial_status;
    uint16_t motor_temperature;
    uint16_t driver_temperature;
    uint8_t reserved[2];
    uint32_t runtime_seconds;
    uint16_t motor_torque;
} UsbDiagnosticMotorState;

typedef struct {
    uint8_t phase;
    uint8_t output_duty_percent;
    int8_t primary_delay_seconds;
    int8_t secondary_delay_seconds;
    int8_t low_threshold_offset;
    int8_t high_threshold_offset;
} UsbDiagnosticCoolingState;

typedef struct {
    uint8_t secondary_duty_percent;
    uint8_t primary_duty_percent;
} UsbDiagnosticPwmState;

typedef struct {
    uint16_t primary;
    uint16_t secondary;
} UsbDiagnosticPulseState;

typedef struct {
    uint8_t direction;
    uint16_t position;
} UsbDiagnosticAuxiliaryPosition;

typedef struct {
    uint8_t base_mode;
    int16_t base_temperatures_c[2];
    uint32_t system_seconds;
    uint32_t transport_error_count;
    UsbDiagnosticMotorState motor;
    WheelStatusSnapshot wheel_status;
    UsbDiagnosticCoolingState cooling;
    UsbDiagnosticPwmState pwm;
    UsbDiagnosticPulseState pulse;
    UsbDiagnosticAuxiliaryPosition auxiliary_position;
    int32_t wheel_position;
    int32_t wheel_velocity;
} UsbDiagnosticSnapshot;

typedef struct {
    uint8_t previous[USB_DEVICE_REPORT_SIZE];
    bool enabled;
    bool dirty;
    bool previous_valid;
} UsbDiagnosticReportService;

void usb_diagnostic_report_service_init(UsbDiagnosticReportService *service);
bool usb_diagnostic_report_apply_command(UsbDiagnosticReportService *service,
                                         const UsbVendorCommand *command);
bool usb_diagnostic_report_prepare(const UsbDiagnosticReportService *service,
                                   const UsbDiagnosticSnapshot *snapshot,
                                   uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_diagnostic_report_commit(UsbDiagnosticReportService *service,
                                  const uint8_t report[USB_DEVICE_REPORT_SIZE]);

#endif
