#ifndef OPENTEC_BASE_USB_PLAYSTATION_WHEEL_VALUE_H
#define OPENTEC_BASE_USB_PLAYSTATION_WHEEL_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

/** @brief Retained PlayStation attached-wheel value and update policy. */
typedef struct {
    uint8_t legacy_axes[2]; /**< Legacy axes in high-byte-first report order. */
    uint32_t deadline_ms;   /**< Expiry deadline for the current nonzero value. */
    bool release_pending;   /**< True when the next release report may replace the value once. */
    bool axis_copy_enabled; /**< True when processed clutch axes are copied continuously. */
} UsbPlaystationWheelValue;

/**
 * @brief Initializes PlayStation wheel-value state.
 *
 * Clears the retained axes, expiry deadline, release latch, and continuous axis-copy gate.
 *
 * @param[out] value Wheel-value state to initialize.
 */
void usb_playstation_wheel_value_init(UsbPlaystationWheelValue *value);

/**
 * @brief Sets a PlayStation attached-wheel value.
 *
 * Stores high before low in legacy report order, arms one release update, and sets the value expiry
 * deadline three seconds after now_ms.
 *
 * @param[in,out] value Wheel-value state to update.
 * @param[in] low Low byte of the protocol value.
 * @param[in] high High byte of the protocol value.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void usb_playstation_wheel_value_set(UsbPlaystationWheelValue *value, uint8_t low, uint8_t high,
                                     uint32_t now_ms);

/**
 * @brief Applies a PlayStation wheel-value output report.
 *
 * A set value flag with a nonzero value replaces the retained axes, refreshes the expiry deadline,
 * and enables continuous axis copying. A subsequent report without that asserted value replaces
 * the axes once when release is pending.
 *
 * @param[in,out] value Wheel-value state to update.
 * @param[in] report Complete PlayStation HID output report.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when report is a valid PlayStation report five; otherwise false.
 */
bool usb_playstation_wheel_value_apply(UsbPlaystationWheelValue *value,
                                       const UsbDeviceOutputReport *report, uint32_t now_ms);

/**
 * @brief Expires a retained PlayStation wheel value.
 *
 * Clears both legacy axes only when they are nonzero and now_ms is strictly after the stored
 * deadline. The release latch and continuous copy gate remain unchanged.
 *
 * @param[in,out] value Wheel-value state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when a nonzero value was cleared; otherwise false.
 */
bool usb_playstation_wheel_value_expire(UsbPlaystationWheelValue *value, uint32_t now_ms);

/**
 * @brief Enables or disables continuous clutch-axis copying.
 *
 * Replaces the persistent axis-copy gate used by usb_playstation_wheel_value_copy_axes.
 *
 * @param[in,out] value Wheel-value state to update.
 * @param[in] enabled True to enable continuous copying; false to disable it.
 */
void usb_playstation_wheel_value_set_axis_copy(UsbPlaystationWheelValue *value, bool enabled);

/**
 * @brief Copies processed clutch axes into the retained wheel value.
 *
 * When axis copying is enabled, stores the source axes in high-byte-first legacy report order
 * without changing the expiry deadline.
 *
 * @param[in,out] value Wheel-value state to update.
 * @param[in] axes Two processed clutch-axis bytes.
 * @return True when value and axes are valid and axis copying is enabled; otherwise false.
 */
bool usb_playstation_wheel_value_copy_axes(UsbPlaystationWheelValue *value, const uint8_t axes[2]);

/**
 * @brief Returns the retained legacy wheel axes.
 *
 * Returns the state-owned two-byte array in high-byte-first report order without changing state.
 *
 * @param[in] value Wheel-value state to inspect.
 * @return Pointer to the retained two-byte axes when value is non-null; otherwise null.
 */
const uint8_t *usb_playstation_wheel_value_axes(const UsbPlaystationWheelValue *value);

#endif
