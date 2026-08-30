#include "usb/tuning_status_report.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    TUNING_STATUS_ROUTE = 8,
    TUNING_STATUS_ENABLE = 1,
    TUNING_STATUS_REFRESH = 2,
    TUNING_STATUS_ENABLED_VALUE = 0xff,
    TUNING_STATUS_HARDWARE_HIGH = 9,
    TUNING_STATUS_PROTOCOL_LOW = 3,
    TUNING_STATUS_PROTOCOL_HIGH = 1,
    TUNING_STATUS_CODE = 0x0d,
};

static void encode_u16(uint8_t output[2], uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static void encode_u32(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

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
