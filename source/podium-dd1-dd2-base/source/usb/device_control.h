#ifndef OPENTEC_BASE_USB_DEVICE_CONTROL_H
#define OPENTEC_BASE_USB_DEVICE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/control_request.h"

enum { USB_DEVICE_INTERFACE_COUNT = 2 };

typedef struct {
    const uint8_t *data;
    uint16_t length;
} UsbDescriptorView;

typedef struct {
    UsbDescriptorView device;
    UsbDescriptorView configuration;
    UsbDescriptorView hid;
    UsbDescriptorView report;
    const UsbDescriptorView *strings;
    uint8_t string_count;
    uint8_t string_alias;
    uint8_t string_alias_target;
} UsbDescriptorCatalog;

typedef enum {
    USB_CONTROL_TRANSFER_STALL,
    USB_CONTROL_TRANSFER_ACKNOWLEDGE,
    USB_CONTROL_TRANSFER_DATA,
    USB_CONTROL_TRANSFER_VALUE,
    USB_CONTROL_TRANSFER_REPORT_IN,
    USB_CONTROL_TRANSFER_REPORT_OUT,
} UsbControlTransferKind;

typedef struct {
    UsbControlTransferKind kind;
    UsbDescriptorView data;
    uint16_t value;
    uint16_t length;
    uint8_t report_type;
    uint8_t report_id;
} UsbControlTransfer;

typedef enum {
    USB_DEVICE_PENDING_NONE,
    USB_DEVICE_PENDING_ADDRESS,
} UsbDevicePendingChange;

typedef struct {
    uint8_t address;
    uint8_t configuration;
    uint8_t alternate_interfaces[USB_DEVICE_INTERFACE_COUNT];
    uint8_t hid_idle_rate;
    uint8_t hid_protocol;
    uint8_t pending_value;
    UsbDevicePendingChange pending_change;
    bool self_powered;
    bool remote_wakeup;
    bool remote_wakeup_forced;
} UsbDeviceControl;

void usb_device_control_init(UsbDeviceControl *device, bool self_powered,
                             bool remote_wakeup_forced);
void usb_device_control_cancel(UsbDeviceControl *device);
UsbControlTransfer usb_device_control_handle(UsbDeviceControl *device,
                                             const UsbControlRequest *request,
                                             const UsbDescriptorCatalog *catalog,
                                             bool endpoint_halted);
void usb_device_control_complete(UsbDeviceControl *device);
bool usb_device_control_configured(const UsbDeviceControl *device);

#endif
