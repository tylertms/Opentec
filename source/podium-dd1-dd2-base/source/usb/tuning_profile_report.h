#ifndef OPENTEC_BASE_USB_TUNING_PROFILE_REPORT_H
#define OPENTEC_BASE_USB_TUNING_PROFILE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "usb/device.h"

/** @brief Number of encoded tuning-profile values in a device-control report. */
enum {
    USB_TUNING_PROFILE_VALUE_COUNT = 25 /**< Number of profile values following the selector. */
};

/**
 * @brief Decodes device-control tuning-profile values.
 *
 * Updates valid fields in the destination profile and retains each current field when its incoming
 * value is outside the protocol range.
 *
 * @param[in] input Twenty-five encoded profile values.
 * @param[in,out] profile Profile state to update.
 * @return True when both input and profile are valid; otherwise false.
 */
bool usb_tuning_profile_report_decode(const uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT],
                                      TuningProfile *profile);

/**
 * @brief Encodes device-control tuning-profile values.
 *
 * Writes the steering-range representation followed by the remaining profile fields in protocol
 * order.
 *
 * @param[in] profile Profile state to encode.
 * @param[out] output Destination for twenty-five encoded values.
 */
void usb_tuning_profile_report_encode(const TuningProfile *profile,
                                      uint8_t output[USB_TUNING_PROFILE_VALUE_COUNT]);

/**
 * @brief Compares two tuning profiles as encoded for the native response.
 *
 * Internal differences that encode to the same twenty-five bytes do not count as a report change.
 *
 * @param[in] previous Profile before an update.
 * @param[in] current Profile after an update.
 * @return True when both profiles are valid and their encoded values differ; otherwise false.
 */
bool usb_tuning_profile_report_changed(const TuningProfile *previous,
                                       const TuningProfile *current);

/**
 * @brief Encodes a complete tuning-profile response.
 *
 * Clears the report, writes the fixed vendor header, combines the one-based active slot with the
 * Standard-mode flag, and appends the active profile values.
 *
 * @param[in] bank Profile bank providing the active slot and mode.
 * @param[out] output Destination for the complete USB report.
 */
void usb_tuning_profile_report_encode_response(const TuningProfileBank *bank,
                                               uint8_t output[USB_DEVICE_REPORT_SIZE]);

#endif
