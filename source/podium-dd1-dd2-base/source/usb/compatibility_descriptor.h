#ifndef OPENTEC_BASE_USB_COMPATIBILITY_DESCRIPTOR_H
#define OPENTEC_BASE_USB_COMPATIBILITY_DESCRIPTOR_H

#include <stdbool.h>

#include "usb/descriptor.h"
#include "usb/input_report.h"

/**
 * @brief Builds the USB descriptor profile for a compatibility operating mode.
 *
 * Selects the device identity and HID configuration used by Fanatec compatibility, Driving Force
 * EX, Driving Force Pro, and G27 modes.
 *
 * @param[in] mode Compatibility operating-mode selector.
 * @param[out] identity Device descriptor fields for the selected mode.
 * @param[out] configuration HID configuration fields for the selected mode.
 * @return True when the mode and output pointers are valid and a profile was built; otherwise
 * false.
 */
bool usb_compatibility_descriptor_profile(UsbInputReportMode mode, UsbDeviceIdentity *identity,
                                          UsbHidConfiguration *configuration);

#endif
