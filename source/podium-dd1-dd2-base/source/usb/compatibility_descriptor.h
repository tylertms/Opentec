#ifndef OPENTEC_BASE_USB_COMPATIBILITY_DESCRIPTOR_H
#define OPENTEC_BASE_USB_COMPATIBILITY_DESCRIPTOR_H

#include <stdbool.h>

#include "usb/descriptor.h"
#include "usb/input_report.h"

bool usb_compatibility_descriptor_profile(UsbInputReportMode mode, UsbDeviceIdentity *identity,
                                          UsbHidConfiguration *configuration);

#endif
