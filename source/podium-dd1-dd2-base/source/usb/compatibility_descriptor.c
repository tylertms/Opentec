#include "usb/compatibility_descriptor.h"

#include <stdbool.h>
#include <stddef.h>

#include "usb/compatibility_report_descriptor.h"

/**
 * @brief Builds the USB descriptor profile for a compatibility operating mode.
 *
 * Selects the device identity and HID configuration used by Fanatec compatibility, Driving Force
 * EX, Driving Force Pro, and G27 modes.
 *
 * @param[in] mode Compatibility operating-mode selector.
 * @param[out] identity Device descriptor fields for the selected mode.
 * @param[out] configuration HID configuration descriptor fields for the selected mode.
 * @return True when the selected mode has a compatibility profile; otherwise false.
 */
bool usb_compatibility_descriptor_profile(UsbInputReportMode mode, UsbDeviceIdentity *identity,
                                          UsbHidConfiguration *configuration) {
    if (identity == NULL || configuration == NULL) {
        return false;
    }

    *identity = (UsbDeviceIdentity){
        .usb_version = 0x0200,
        .device_version = 0x0059,
        .control_packet_size = 64,
    };
    *configuration = (UsbHidConfiguration){
        .country_code = 0x21,
        .input_endpoint = 0x81,
        .output_endpoint = 0x01,
        .maximum_power_ma = 98,
    };

    switch (mode) {
    case USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY:
        identity->vendor_id = 0x0eb7;
        identity->product_id = 0x0e03;
        identity->manufacturer_string = 1;
        identity->product_string = 8;
        configuration->hid_version = 0x0111;
        configuration->report_descriptor_size = USB_FANATEC_COMPATIBILITY_REPORT_DESCRIPTOR_SIZE;
        configuration->endpoint_packet_size = 64;
        configuration->maximum_power_ma = 80;
        configuration->poll_interval_ms = 1;
        configuration->self_powered = true;
        return true;

    case USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX:
        identity->vendor_id = 0x046d;
        identity->product_id = 0xc294;
        identity->product_string = 2;
        configuration->hid_version = 0x0100;
        configuration->report_descriptor_size = USB_DRIVING_FORCE_EX_REPORT_DESCRIPTOR_SIZE;
        configuration->endpoint_packet_size = 8;
        configuration->poll_interval_ms = 10;
        return true;

    case USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO:
        identity->vendor_id = 0x046d;
        identity->product_id = 0xc298;
        identity->product_string = 2;
        configuration->hid_version = 0x0111;
        configuration->report_descriptor_size = USB_DRIVING_FORCE_PRO_REPORT_DESCRIPTOR_SIZE;
        configuration->endpoint_packet_size = 8;
        configuration->maximum_power_ma = 80;
        configuration->poll_interval_ms = 2;
        configuration->interface_string = 0xfe;
        return true;

    case USB_INPUT_REPORT_MODE_G27:
        identity->vendor_id = 0x046d;
        identity->product_id = 0xc29b;
        identity->device_version = 0x1238;
        identity->product_string = 2;
        configuration->hid_version = 0x0111;
        configuration->report_descriptor_size = USB_G27_REPORT_DESCRIPTOR_SIZE;
        configuration->endpoint_packet_size = 16;
        configuration->poll_interval_ms = 2;
        configuration->interface_string = 0xfe;
        return true;

    default:
        return false;
    }
}
