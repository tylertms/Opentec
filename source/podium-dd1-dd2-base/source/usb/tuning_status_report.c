#include "usb/tuning_status_report.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Tuning-status command, report, and protocol constants. */
enum {
    TUNING_STATUS_ROUTE = 8,            /**< Vendor command route for tuning status. */
    TUNING_STATUS_ENABLE = 1,           /**< Enable or disable action. */
    TUNING_STATUS_REFRESH = 2,          /**< Explicit refresh action. */
    TUNING_STATUS_ENABLED_VALUE = 0xff, /**< Argument value that enables publication. */
    TUNING_STATUS_HARDWARE_HIGH = 9,    /**< Fixed hardware-status field value. */
    TUNING_STATUS_PROTOCOL_LOW = 3,     /**< Fixed protocol field low value. */
    TUNING_STATUS_PROTOCOL_HIGH = 1,    /**< Fixed protocol field high value. */
    TUNING_STATUS_CODE = 0x0d,          /**< Fixed tuning-status report code. */
    TUNING_STATUS_OUTPUT_STRENGTH_LIMIT = 99, /**< Highest non-saturated strength. */
    TUNING_STATUS_OUTPUT_ACTIVE = 1,           /**< Active output-status value. */
    TUNING_STATUS_OUTPUT_SATURATED = 0x0f,     /**< Saturated output-status value. */
    TUNING_STATUS_DD1_STRENGTH_PERCENT = 40,   /**< Reduced DD1 runtime strength. */
    TUNING_STATUS_DD2_STRENGTH_PERCENT = 32,   /**< Reduced DD2 runtime strength. */
};

/**
 * @brief Encodes one little-endian 16-bit status value.
 *
 * Writes the low byte followed by the high byte without alignment requirements.
 *
 * @param[out] output Two-byte destination.
 * @param[in] value Value to encode.
 */
static void encode_u16(uint8_t output[2], uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Encodes one little-endian 32-bit status value.
 *
 * Writes all four bytes from least significant to most significant.
 *
 * @param[out] output Four-byte destination.
 * @param[in] value Value to encode.
 */
static void encode_u32(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Encodes the complete vendor tuning-status report.
 *
 * Clears the 64-byte payload and writes the fixed route, protocol, and status fields together with
 * current base, wheel, adapter, pedal, input, tuning, output, and interface state.
 *
 * @param[in] snapshot Current tuning-status sources.
 * @param[out] output Encoded 64-byte report.
 */
static void encode_report(const UsbTuningStatusSnapshot *snapshot,
                          uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = UINT8_MAX;
    output[1] = TUNING_STATUS_ROUTE;
    encode_u16(output + 2, snapshot->base_status);
    output[4] = snapshot->hardware_mode;
    output[5] = TUNING_STATUS_HARDWARE_HIGH;
    output[6] = TUNING_STATUS_PROTOCOL_LOW;
    output[7] = TUNING_STATUS_PROTOCOL_HIGH;
    output[8] = 1;
    output[10] = snapshot->auxiliary_mode;
    output[11] = snapshot->auxiliary_flags;
    encode_u32(output + 12, snapshot->auxiliary_status);
    output[17] = 1;
    output[18] = snapshot->wheel_status_low;
    output[19] = snapshot->wheel_status_high & 0x3fu;
    output[24] = snapshot->wheel_mode;
    output[25] = snapshot->button_mode;
    encode_u32(output + 26, snapshot->input);
    output[31] = snapshot->adapter_mode;
    memcpy(output + 32, snapshot->adapter, sizeof(snapshot->adapter));
    output[38] = snapshot->pedal_status;
    output[39] = snapshot->pedal_auxiliary;
    output[40] = snapshot->pedal_axis_low;
    output[41] = snapshot->pedal_axis_high;
    output[45] = snapshot->tuning_available ? 1 : 0;
    output[46] = snapshot->system_active ? 1 : 0;
    output[48] = snapshot->force_effect & 0x7fu;
    if (output[48] != 0) {
        output[48] |= 0x80u;
    }
    output[50] = snapshot->system_flags;
    output[53] = TUNING_STATUS_CODE;
    output[55] = snapshot->output_status;
    output[56] = snapshot->interface_gate ? 1 : 0;
}

uint8_t usb_tuning_status_report_output_status(bool force_output_override_requested,
                                               bool wheel_protocol_past_selection,
                                               bool primary_output_disabled,
                                               uint32_t runtime_strength_percent) {
    if (force_output_override_requested ||
        (!wheel_protocol_past_selection && !primary_output_disabled)) {
        return 0;
    }
    if (runtime_strength_percent > TUNING_STATUS_OUTPUT_STRENGTH_LIMIT) {
        return TUNING_STATUS_OUTPUT_SATURATED;
    }
    return runtime_strength_percent == TUNING_STATUS_DD1_STRENGTH_PERCENT ||
                   runtime_strength_percent == TUNING_STATUS_DD2_STRENGTH_PERCENT
               ? TUNING_STATUS_OUTPUT_ACTIVE
               : 0;
}

void usb_tuning_status_report_service_init(UsbTuningStatusReportService *service) {
    if (service != 0) {
        *service = (UsbTuningStatusReportService){0};
    }
}

bool usb_tuning_status_report_apply_command(UsbTuningStatusReportService *service,
                                            const UsbVendorCommand *command) {
    if (service == 0 || command == 0 || command->kind != USB_VENDOR_COMMAND_TUNING_STATUS ||
        command->arguments == 0 || command->length < 2) {
        return false;
    }
    if (command->arguments[0] == TUNING_STATUS_ENABLE) {
        service->enabled = command->arguments[1] == TUNING_STATUS_ENABLED_VALUE;
    } else if (command->arguments[0] == TUNING_STATUS_REFRESH) {
        service->dirty = true;
    }
    return true;
}

bool usb_tuning_status_report_prepare(const UsbTuningStatusReportService *service,
                                      const UsbTuningStatusSnapshot *snapshot,
                                      uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    if (service == 0 || snapshot == 0 || output == 0 || !service->enabled) {
        return false;
    }
    encode_report(snapshot, output);
    return service->dirty || !service->previous_valid ||
           memcmp(output, service->previous, USB_DEVICE_REPORT_SIZE) != 0;
}

void usb_tuning_status_report_commit(UsbTuningStatusReportService *service,
                                     const uint8_t report[USB_DEVICE_REPORT_SIZE]) {
    if (service == 0 || report == 0) {
        return;
    }
    memcpy(service->previous, report, USB_DEVICE_REPORT_SIZE);
    service->previous_valid = true;
    service->dirty = false;
}
