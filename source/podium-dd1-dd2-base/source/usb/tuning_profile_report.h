#ifndef OPENTEC_BASE_USB_TUNING_PROFILE_REPORT_H
#define OPENTEC_BASE_USB_TUNING_PROFILE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"

enum { USB_TUNING_PROFILE_VALUE_COUNT = 25 };

bool usb_tuning_profile_report_decode(const uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT],
                                      TuningProfile *profile);
void usb_tuning_profile_report_encode(const TuningProfile *profile,
                                      uint8_t output[USB_TUNING_PROFILE_VALUE_COUNT]);

#endif
