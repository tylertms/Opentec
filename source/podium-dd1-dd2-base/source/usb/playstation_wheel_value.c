#include "usb/playstation_wheel_value.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    PLAYSTATION_WHEEL_VALUE_REPORT_ID = 5,
    PLAYSTATION_WHEEL_VALUE_REPORT_SIZE = 64,
    PLAYSTATION_WHEEL_VALUE_FLAG = 0x01,
    PLAYSTATION_WHEEL_VALUE_FLAGS_OFFSET = 1,
    PLAYSTATION_WHEEL_VALUE_LOW_OFFSET = 4,
    PLAYSTATION_WHEEL_VALUE_HIGH_OFFSET = 5,
    PLAYSTATION_WHEEL_VALUE_TIMEOUT_MS = 3000,
};

/**
 * @brief Initializes the PlayStation wheel-value state.
 *
 * Clears both attached-wheel legacy axes, the expiry deadline, and the pending release latch.
 *
 * @param[out] value Wheel-value state to initialize.
 */
void usb_playstation_wheel_value_init(UsbPlaystationWheelValue *value) {
    if (value == NULL) {
        return;
    }
    *value = (UsbPlaystationWheelValue){0};
}

/**
 * @brief Sets the attached-wheel protocol value and refreshes its timeout.
 *
 * Stores the high byte before the low byte in attached-wheel report order, arms the release latch,
 * and starts the same three-second lifetime used by PlayStation report five.
 *
 * @param[in,out] value Wheel-value state to replace.
 * @param[in] low Low protocol-value byte.
 * @param[in] high High protocol-value byte.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void usb_playstation_wheel_value_set(UsbPlaystationWheelValue *value, uint8_t low, uint8_t high,
                                     uint32_t now_ms) {
    if (value == NULL) {
        return;
    }
    value->legacy_axes[0] = high;
    value->legacy_axes[1] = low;
    value->deadline_ms = now_ms + PLAYSTATION_WHEEL_VALUE_TIMEOUT_MS;
    value->release_pending = true;
}

/**
 * @brief Applies a PlayStation wheel-value output report.
 *
 * An asserted value flag with either nonzero value byte replaces the attached-wheel legacy axes,
 * refreshes the three-second deadline, and arms one release update. The next report without an
 * asserted nonzero value applies its value once and releases that latch.
 *
 * @param[in,out] value Current wheel-value state.
 * @param[in] report Complete PlayStation HID output report.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when report 5 was recognized; otherwise false.
 */
bool usb_playstation_wheel_value_apply(UsbPlaystationWheelValue *value,
                                       const UsbDeviceOutputReport *report, uint32_t now_ms) {
    if (value == NULL || report == NULL || report->report_type != USB_DEVICE_HID_REPORT_OUTPUT ||
        report->report_id != PLAYSTATION_WHEEL_VALUE_REPORT_ID ||
        report->length != PLAYSTATION_WHEEL_VALUE_REPORT_SIZE ||
        report->data[0] != PLAYSTATION_WHEEL_VALUE_REPORT_ID) {
        return false;
    }

    uint8_t low = report->data[PLAYSTATION_WHEEL_VALUE_LOW_OFFSET];
    uint8_t high = report->data[PLAYSTATION_WHEEL_VALUE_HIGH_OFFSET];
    bool asserted =
        (report->data[PLAYSTATION_WHEEL_VALUE_FLAGS_OFFSET] & PLAYSTATION_WHEEL_VALUE_FLAG) != 0 &&
        (low != 0 || high != 0);
    if (asserted) {
        value->legacy_axes[0] = high;
        value->legacy_axes[1] = low;
        value->deadline_ms = now_ms + PLAYSTATION_WHEEL_VALUE_TIMEOUT_MS;
        value->release_pending = true;
        value->axis_copy_enabled = true;
    } else if (value->release_pending) {
        value->legacy_axes[0] = high;
        value->legacy_axes[1] = low;
        value->release_pending = false;
    }
    return true;
}

/**
 * @brief Selects continuous attached-wheel axis copying.
 *
 * Retains the normalized gate until another host command replaces it.
 *
 * @param[in,out] value Current wheel-value state.
 * @param[in] enabled True to copy processed attached-wheel axes continuously.
 */
void usb_playstation_wheel_value_set_axis_copy(UsbPlaystationWheelValue *value, bool enabled) {
    if (value != NULL) {
        value->axis_copy_enabled = enabled;
    }
}

/**
 * @brief Copies processed attached-wheel axes into the protocol value.
 *
 * While the persistent gate is enabled, swaps the two source axes into the high-byte-first legacy
 * protocol order without changing the host-command expiry deadline.
 *
 * @param[in,out] value Current wheel-value state.
 * @param[in] axes Two processed attached-wheel axis bytes.
 * @return True when the gated axes were copied.
 */
bool usb_playstation_wheel_value_copy_axes(UsbPlaystationWheelValue *value, const uint8_t axes[2]) {
    if (value == NULL || axes == NULL || !value->axis_copy_enabled) {
        return false;
    }
    value->legacy_axes[0] = axes[1];
    value->legacy_axes[1] = axes[0];
    return true;
}

/**
 * @brief Expires an inactive PlayStation wheel value.
 *
 * Clears a nonzero legacy-axis pair after its three-second deadline. The pending release latch is
 * retained so a later release report can still publish its supplied value once.
 *
 * @param[in,out] value Current wheel-value state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when a nonzero axis pair was cleared; otherwise false.
 */
bool usb_playstation_wheel_value_expire(UsbPlaystationWheelValue *value, uint32_t now_ms) {
    if (value == NULL || now_ms <= value->deadline_ms ||
        (value->legacy_axes[0] == 0 && value->legacy_axes[1] == 0)) {
        return false;
    }
    value->legacy_axes[0] = 0;
    value->legacy_axes[1] = 0;
    return true;
}

/**
 * @brief Returns the current attached-wheel legacy axes.
 *
 * Exposes the high-byte axis first and low-byte axis second in attached-wheel response order.
 *
 * @param[in] value Current wheel-value state.
 * @return Two-byte axis array, or null when the state is null.
 */
const uint8_t *usb_playstation_wheel_value_axes(const UsbPlaystationWheelValue *value) {
    return value == NULL ? NULL : value->legacy_axes;
}
