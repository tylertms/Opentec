#include "usb/output_command.h"

#include <stddef.h>
#include <stdint.h>

/** @brief Internal report identifiers and payload sizes accepted by output decoding. */
enum {
    NATIVE_PRIMARY_REPORT_ID = 1,                 /**< Native primary short-report identifier. */
    NATIVE_EXTENDED_REPORT_ID = 2,                /**< Native extended short-report identifier. */
    SHORT_REPORT_PAYLOAD_SIZE = 7,                /**< Short command payload size in bytes. */
    VENDOR_TRANSFER_REPORT_ID = 0xff,             /**< Native vendor-transfer report identifier. */
    VENDOR_TRANSFER_PAYLOAD_SIZE = 63,            /**< Vendor-transfer payload size in bytes. */
    PLAYSTATION_REPORT_ID = 0x30,                 /**< PlayStation full-report identifier. */
    PLAYSTATION_VENDOR_TRANSFER_REPORT_ID = 0x34, /**< PlayStation vendor-transfer identifier. */
    PLAYSTATION_REPORT_SIZE = 64,                 /**< PlayStation report size in bytes. */
};

static bool is_native_short_report_id(uint8_t report_id) {
    return report_id == NATIVE_PRIMARY_REPORT_ID || report_id == NATIVE_EXTENDED_REPORT_ID;
}

bool usb_output_command_decode(const UsbDeviceOutputReport *report, UsbOutputCommand *command) {
    if (report == NULL || command == NULL || report->report_type != USB_DEVICE_HID_REPORT_OUTPUT ||
        report->length == 0) {
        return false;
    }

    if (report->report_id == 0 && report->length == SHORT_REPORT_PAYLOAD_SIZE) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_SHORT,
            .payload = report->data,
            .length = SHORT_REPORT_PAYLOAD_SIZE,
        };
        return true;
    }

    if (report->report_id == NATIVE_EXTENDED_REPORT_ID &&
        report->length == SHORT_REPORT_PAYLOAD_SIZE) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_SHORT,
            .payload = report->data,
            .length = SHORT_REPORT_PAYLOAD_SIZE,
        };
        return true;
    }

    if (report->data[0] != report->report_id) {
        return false;
    }

    if (is_native_short_report_id(report->report_id) &&
        report->length == SHORT_REPORT_PAYLOAD_SIZE + 1) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_SHORT,
            .payload = report->data + 1,
            .length = SHORT_REPORT_PAYLOAD_SIZE,
        };
        return true;
    }

    if (report->report_id == VENDOR_TRANSFER_REPORT_ID &&
        report->length == VENDOR_TRANSFER_PAYLOAD_SIZE + 1) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
            .payload = report->data + 1,
            .length = VENDOR_TRANSFER_PAYLOAD_SIZE,
        };
        return true;
    }

    if ((report->report_id == PLAYSTATION_REPORT_ID ||
         report->report_id == PLAYSTATION_VENDOR_TRANSFER_REPORT_ID) &&
        report->length == PLAYSTATION_REPORT_SIZE) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
            .payload = report->data + 1,
            .length = VENDOR_TRANSFER_PAYLOAD_SIZE,
        };
        return true;
    }

    return false;
}
