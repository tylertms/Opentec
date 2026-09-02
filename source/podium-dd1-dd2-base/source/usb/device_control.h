#ifndef OPENTEC_BASE_USB_DEVICE_CONTROL_H
#define OPENTEC_BASE_USB_DEVICE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/control_request.h"

/** @brief Number of USB interfaces exposed by the endpoint-zero service. */
enum { USB_DEVICE_INTERFACE_COUNT = 2 /**< Number of exposed USB interfaces. */ };

/** @brief Bounded view of a descriptor byte sequence. */
typedef struct {
    const uint8_t *data; /**< Descriptor bytes. */
    uint16_t length;     /**< Number of descriptor bytes available. */
} UsbDescriptorView;

/** @brief Device, configuration, HID, report, and string descriptors for enumeration. */
typedef struct {
    UsbDescriptorView device;         /**< USB device descriptor. */
    UsbDescriptorView configuration;  /**< USB configuration descriptor. */
    UsbDescriptorView hid;            /**< HID class descriptor. */
    UsbDescriptorView report;         /**< HID report descriptor. */
    const UsbDescriptorView *strings; /**< String descriptor table. */
    uint8_t string_count;             /**< Number of entries in the string table. */
    bool string_alias_valid;          /**< True when the configured string alias is active. */
    uint8_t string_alias;             /**< Descriptor index that aliases another string. */
    uint8_t string_alias_target;      /**< Backing string index exposed only through the alias. */
} UsbDescriptorCatalog;

/** @brief Transfer kinds returned by endpoint-zero request handling. */
typedef enum {
    USB_CONTROL_TRANSFER_STALL,       /**< Stall the control transfer. */
    USB_CONTROL_TRANSFER_ACKNOWLEDGE, /**< Complete the status stage without data. */
    USB_CONTROL_TRANSFER_DATA,        /**< Send descriptor data through the control pipe. */
    USB_CONTROL_TRANSFER_VALUE,       /**< Send a short scalar value. */
    USB_CONTROL_TRANSFER_REPORT_IN,   /**< Send a device-to-host HID report. */
    USB_CONTROL_TRANSFER_REPORT_OUT,  /**< Receive a host-to-device HID report. */
} UsbControlTransferKind;

/** @brief Endpoint-zero transfer description selected for one classified request. */
typedef struct {
    UsbControlTransferKind kind; /**< Selected transfer operation. */
    UsbDescriptorView data;      /**< Descriptor bytes for a data transfer. */
    uint16_t value;              /**< Scalar value for a value transfer. */
    uint16_t length;             /**< Number of bytes in the selected transfer. */
    uint8_t report_type;         /**< HID report type for a report transfer. */
    uint8_t report_id;           /**< HID report identifier for a report transfer. */
} UsbControlTransfer;

/** @brief Deferred device-state changes applied after a control status stage. */
typedef enum {
    USB_DEVICE_PENDING_NONE,    /**< No deferred state change. */
    USB_DEVICE_PENDING_ADDRESS, /**< Apply a pending device address. */
} UsbDevicePendingChange;

/** @brief USB address, configuration, interface, HID, power, and pending-change state. */
typedef struct {
    uint8_t address;       /**< Current USB device address. */
    uint8_t configuration; /**< Active USB configuration value. */
    uint8_t alternate_interfaces[USB_DEVICE_INTERFACE_COUNT]; /**< Active alternate settings. */
    uint8_t hid_idle_rate;                 /**< HID idle rate for interface zero. */
    uint8_t hid_protocol;                  /**< HID protocol selected by the host. */
    uint8_t pending_value;                 /**< Value retained for the pending state change. */
    UsbDevicePendingChange pending_change; /**< Deferred state change kind. */
    bool self_powered;         /**< True when the active configuration is self-powered. */
    bool remote_wakeup;        /**< True when the host enabled remote wakeup. */
    bool remote_wakeup_forced; /**< True when hardware requires remote wakeup status. */
} UsbDeviceControl;

/**
 * @brief Initializes endpoint-zero device state.
 *
 * Clears address, configuration, interface, HID, and pending-change state while retaining the
 * supplied device capability flags.
 *
 * @param[out] device Endpoint-zero state to initialize.
 * @param[in] self_powered True when the active configuration is self-powered.
 * @param[in] remote_wakeup_forced True when status must advertise remote wakeup independently of
 * the host feature state.
 */
void usb_device_control_init(UsbDeviceControl *device, bool self_powered,
                             bool remote_wakeup_forced);

/**
 * @brief Cancels a deferred endpoint-zero state change.
 *
 * Clears the pending address operation and its retained value when a new setup transaction starts.
 *
 * @param[in,out] device Current endpoint-zero state.
 */
void usb_device_control_cancel(UsbDeviceControl *device);

/**
 * @brief Handles a classified endpoint-zero request.
 *
 * Applies standard device state changes, serves descriptors and HID state, and describes the
 * transfer that the control pipe must perform.
 *
 * @param[in,out] device Current USB device state.
 * @param[in] request Classified control request.
 * @param[in] catalog Active descriptor catalog.
 * @param[in] endpoint_halted Current halt state for the request endpoint.
 * @return Control transfer selected for the request.
 */
UsbControlTransfer usb_device_control_handle(UsbDeviceControl *device,
                                             const UsbControlRequest *request,
                                             const UsbDescriptorCatalog *catalog,
                                             bool endpoint_halted);

/**
 * @brief Commits a pending USB address.
 *
 * Applies the retained address after the status stage and clears the pending transition.
 *
 * @param[in,out] device Current USB device state.
 */
void usb_device_control_complete(UsbDeviceControl *device);

/**
 * @brief Reports whether the USB device has an active configuration.
 *
 * Treats every nonzero configuration value as configured, matching endpoint request gating.
 *
 * @param[in] device Current endpoint-zero state.
 * @return True when the configuration value is nonzero; otherwise false.
 */
bool usb_device_control_configured(const UsbDeviceControl *device);

#endif
