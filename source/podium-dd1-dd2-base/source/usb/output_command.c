#include "usb/output_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    OPERATING_MODE_REPORT_ID = 1,
    OPERATING_MODE_PAYLOAD_SIZE = 7,
    VENDOR_TRANSFER_REPORT_ID = 0xff,
    VENDOR_TRANSFER_PAYLOAD_SIZE = 63,
};

/**
 * Classifies a complete Podium HID output report and exposes its command payload.
 *
 * @param report HID output report including its leading report ID.
 * @param command Destination for the command kind, payload, and payload length.
 * @return True for the short operating-mode report or the full vendor-transfer report.
 */
bool usb_output_command_decode(const UsbDeviceOutputReport *report, UsbOutputCommand *command) {
    if (report == NULL || command == NULL || report->report_type != USB_DEVICE_HID_REPORT_OUTPUT ||
        report->length == 0 || report->data[0] != report->report_id) {
        return false;
    }

    if (report->report_id == OPERATING_MODE_REPORT_ID &&
        report->length == OPERATING_MODE_PAYLOAD_SIZE + 1) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_OPERATING_MODE,
            .payload = report->data + 1,
            .length = OPERATING_MODE_PAYLOAD_SIZE,
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

    return false;
}
