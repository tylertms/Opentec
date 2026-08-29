#include "usb/output_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SHORT_REPORT_ID = 1,
    SHORT_REPORT_PAYLOAD_SIZE = 7,
    VENDOR_TRANSFER_REPORT_ID = 0xff,
    VENDOR_TRANSFER_PAYLOAD_SIZE = 63,
    PLAYSTATION_SHORT_REPORT_ID = 0x30,
    PLAYSTATION_VENDOR_TRANSFER_REPORT_ID = 0x34,
    PLAYSTATION_REPORT_SIZE = 64,
};

/**
 * @brief Classifies a complete primary HID output report.
 *
 * Exposes a seven-byte shared command from an unnumbered compatibility report, native report 1,
 * or PlayStation report 48. Native report 255 and PlayStation report 52 expose the same 63-byte
 * vendor-transfer payload.
 *
 * @param[in] report HID output report with its endpoint-specific report-ID classification.
 * @param[out] command Destination for the command kind, payload, and payload length.
 * @return True for the shared short report or the full vendor-transfer report.
 */
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

    if (report->data[0] != report->report_id) {
        return false;
    }

    if (report->report_id == SHORT_REPORT_ID && report->length == SHORT_REPORT_PAYLOAD_SIZE + 1) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_SHORT,
            .payload = report->data + 1,
            .length = SHORT_REPORT_PAYLOAD_SIZE,
        };
        return true;
    }

    if (report->report_id == PLAYSTATION_SHORT_REPORT_ID &&
        report->length == PLAYSTATION_REPORT_SIZE) {
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

    if (report->report_id == PLAYSTATION_VENDOR_TRANSFER_REPORT_ID &&
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
