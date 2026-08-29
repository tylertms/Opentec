#ifndef OPENTEC_BASE_USB_VENDOR_COMMAND_H
#define OPENTEC_BASE_USB_VENDOR_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"
#include "wheel/transfer_service.h"

typedef enum {
    USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT,
    USB_VENDOR_COMMAND_TUNING_MENU,
    USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE,
    USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT,
    USB_VENDOR_COMMAND_REMOTE_TUNING,
    USB_VENDOR_COMMAND_STATUS_RESPONSE,
    USB_VENDOR_COMMAND_SCRIPT_AXES,
    USB_VENDOR_COMMAND_SCRIPT_SAMPLES,
    USB_VENDOR_COMMAND_SCRIPT_STATUS,
    USB_VENDOR_COMMAND_SCRIPT_VALUES,
    USB_VENDOR_COMMAND_EXTENDED_RESET,
    USB_VENDOR_COMMAND_EDS_WRITE,
    USB_VENDOR_COMMAND_EDS_TRANSFER,
    USB_VENDOR_COMMAND_EXTENDED,
} UsbVendorCommandKind;

typedef struct {
    UsbVendorCommandKind kind;
    uint8_t opcode;
    const uint8_t *arguments;
    uint8_t length;
} UsbVendorCommand;

typedef enum {
    USB_WHEEL_TRANSFER_START = 1,
    USB_WHEEL_TRANSFER_STATUS = 2,
} UsbWheelTransferAction;

typedef struct {
    WheelTransferRequest request;
    UsbWheelTransferAction action;
} UsbWheelTransferCommand;

bool usb_vendor_command_decode(const UsbOutputCommand *output, UsbVendorCommand *command);
bool usb_vendor_command_script_sample_index(const UsbVendorCommand *command, uint16_t *index);
bool usb_vendor_command_requests_motor_command(const UsbVendorCommand *command);
bool usb_vendor_command_decode_wheel_transfer(const UsbVendorCommand *command,
                                              UsbWheelTransferCommand *transfer);
const uint8_t *usb_vendor_command_decode_wheel_report_seventeen(const UsbVendorCommand *command);
void usb_vendor_command_encode_wheel_transfer_response(WheelTransferRequest request,
                                                       WheelTransferStatus status,
                                                       uint8_t output[USB_DEVICE_REPORT_SIZE]);

#endif
