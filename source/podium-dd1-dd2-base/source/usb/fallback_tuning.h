#ifndef OPENTEC_BASE_USB_FALLBACK_TUNING_H
#define OPENTEC_BASE_USB_FALLBACK_TUNING_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"
#include "usb/fallback_command.h"

bool usb_fallback_tuning_range_allowed(const TuningProfile *profile);
bool usb_fallback_tuning_apply(const UsbFallbackCommand *command, uint8_t active_slot,
                               TuningProfile *profile);

#endif
