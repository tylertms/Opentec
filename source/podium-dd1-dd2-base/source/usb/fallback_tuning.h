#ifndef OPENTEC_BASE_USB_FALLBACK_TUNING_H
#define OPENTEC_BASE_USB_FALLBACK_TUNING_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"
#include "usb/fallback_command.h"

/**
 * @brief Tests whether fallback steering-range commands may replace the active range.
 *
 * Accepts the official manual 1300-degree sensitivity and automatic sensitivity selections.
 *
 * @param[in] profile Active runtime tuning profile.
 * @return True when a fallback range command may be applied; otherwise false.
 */
bool usb_fallback_tuning_range_allowed(const TuningProfile *profile);

/**
 * @brief Applies one transient setup-one fallback tuning command.
 *
 * Changes only the supplied runtime profile and applies the official command-specific limits.
 *
 * @param[in] command Decoded fallback tuning command.
 * @param[in] active_slot Zero-based active tuning setup.
 * @param[in,out] profile Runtime tuning profile to update.
 * @return True when the command applies to the active setup; otherwise false.
 */
bool usb_fallback_tuning_apply(const UsbFallbackCommand *command, uint8_t active_slot,
                               TuningProfile *profile);

#endif
