#ifndef OPENTEC_BASE_USB_TUNING_PROFILE_REPORT_H
#define OPENTEC_BASE_USB_TUNING_PROFILE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "usb/device.h"

enum { USB_TUNING_PROFILE_VALUE_COUNT = 25 };

bool usb_tuning_profile_report_decode(const uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT],
                                      TuningProfile *profile);
void usb_tuning_profile_report_encode(const TuningProfile *profile,
                                      uint8_t output[USB_TUNING_PROFILE_VALUE_COUNT]);
void usb_tuning_profile_report_encode_response(const TuningProfileBank *bank,
                                               uint8_t output[USB_DEVICE_REPORT_SIZE]);

#endif
