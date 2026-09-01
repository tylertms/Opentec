#include "usb/diagnostic_report.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/** @brief Diagnostic vendor-report identifiers and command values. */
enum {
    DIAGNOSTIC_REPORT_IDENTIFIER = 0x0903,  /**< Diagnostic report identifier. */
    DIAGNOSTIC_REPORT_ROUTE = 4,            /**< Diagnostic vendor route number. */
    DIAGNOSTIC_REPORT_ENABLE = 1,           /**< Diagnostic enable command. */
    DIAGNOSTIC_REPORT_REFRESH = 2,          /**< Diagnostic refresh command. */
    DIAGNOSTIC_REPORT_ENABLED_VALUE = 0xff, /**< Command value that enables publication. */
};

/**
 * @brief Writes a little-endian 16-bit diagnostic value.
 *
 * Stores both bytes consecutively with the least-significant byte first.
 *
 * @param[out] output First destination byte.
 * @param[in] value Value to encode.
 */
static void encode_u16(uint8_t output[2], uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Writes a little-endian 32-bit diagnostic value.
 *
 * Stores all four bytes consecutively with the least-significant byte first.
 *
 * @param[out] output First destination byte.
 * @param[in] value Value to encode.
 */
static void encode_u32(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Encodes the complete diagnostic vendor report.
 *
 * Builds route FF 04 and maps the logical snapshot into the 62-byte diagnostic payload. The
 * transmitted payload ends after the low byte of wheel velocity.
 *
 * @param[in] snapshot Logical diagnostic values to encode.
 * @param[out] output Encoded 64-byte vendor report.
 */
static void encode_report(const UsbDiagnosticSnapshot *snapshot,
                          uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = UINT8_MAX;
    output[1] = DIAGNOSTIC_REPORT_ROUTE;
    encode_u16(output + 2, DIAGNOSTIC_REPORT_IDENTIFIER);
    output[4] = snapshot->base_mode;
    encode_u16(output + 5, (uint16_t)snapshot->base_temperatures_c[0]);
    encode_u16(output + 7, (uint16_t)snapshot->base_temperatures_c[1]);
    encode_u32(output + 9, snapshot->system_seconds);
    encode_u32(output + 13, snapshot->transport_error_count);
    output[17] = snapshot->motor.version;
    output[18] = (uint8_t)snapshot->motor.initial_status;
    encode_u16(output + 19, snapshot->motor.motor_temperature);
    encode_u16(output + 21, snapshot->motor.driver_temperature);
    output[23] = snapshot->motor.reserved[0];
    output[24] = snapshot->motor.reserved[1];
    encode_u32(output + 25, snapshot->motor.runtime_seconds);
    encode_u16(output + 29, snapshot->motor.motor_torque);
    output[31] = snapshot->wheel_status.status_high;
    output[32] = snapshot->wheel_status.status_low;
    encode_u16(output + 33, snapshot->wheel_status.accessory_value);
    encode_u32(output + 35, snapshot->wheel_status.runtime_seconds);
    encode_u32(output + 39, snapshot->wheel_status.runtime_counter);
    output[43] = snapshot->wheel_status.trailing_status;
    output[44] = snapshot->cooling.phase;
    output[45] = snapshot->cooling.output_duty_percent;
    output[46] = (uint8_t)snapshot->cooling.primary_delay_seconds;
    output[47] = (uint8_t)snapshot->cooling.secondary_delay_seconds;
    output[48] = (uint8_t)snapshot->cooling.low_threshold_offset;
    output[49] = (uint8_t)snapshot->cooling.high_threshold_offset;
    output[50] = snapshot->pwm.secondary_duty_percent;
    output[51] = snapshot->pwm.primary_duty_percent;
    encode_u16(output + 52, snapshot->pulse.primary);
    encode_u16(output + 54, snapshot->pulse.secondary);
    output[56] = snapshot->auxiliary_position.direction;
    encode_u16(output + 57, snapshot->auxiliary_position.position);
    encode_u32(output + 59, (uint32_t)snapshot->wheel_position);
    output[63] = (uint8_t)snapshot->wheel_velocity;
}

void usb_diagnostic_report_service_init(UsbDiagnosticReportService *service) {
    if (service != 0) {
        *service = (UsbDiagnosticReportService){0};
    }
}

bool usb_diagnostic_report_apply_command(UsbDiagnosticReportService *service,
                                         const UsbVendorCommand *command) {
    if (service == 0 || command == 0 || command->kind != USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT ||
        command->arguments == 0 || command->length < 2) {
        return false;
    }
    if (command->arguments[0] == DIAGNOSTIC_REPORT_ENABLE) {
        service->enabled = command->arguments[1] == DIAGNOSTIC_REPORT_ENABLED_VALUE;
    } else if (command->arguments[0] == DIAGNOSTIC_REPORT_REFRESH) {
        service->dirty = true;
    }
    return true;
}

bool usb_diagnostic_report_prepare(const UsbDiagnosticReportService *service,
                                   const UsbDiagnosticSnapshot *snapshot,
                                   uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    if (service == 0 || snapshot == 0 || output == 0 || !service->enabled) {
        return false;
    }
    encode_report(snapshot, output);
    return service->dirty || !service->previous_valid ||
           memcmp(output, service->previous, USB_DEVICE_REPORT_SIZE) != 0;
}

void usb_diagnostic_report_commit(UsbDiagnosticReportService *service,
                                  const uint8_t report[USB_DEVICE_REPORT_SIZE]) {
    if (service == 0 || report == 0) {
        return;
    }
    memcpy(service->previous, report, USB_DEVICE_REPORT_SIZE);
    service->previous_valid = true;
    service->dirty = false;
}
