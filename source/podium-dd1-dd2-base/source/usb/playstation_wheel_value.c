#include "usb/playstation_wheel_value.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Internal report layout values for PlayStation wheel-value updates. */
enum {
    PLAYSTATION_WHEEL_VALUE_REPORT_ID = 5,     /**< Wheel-value output report identifier. */
    PLAYSTATION_WHEEL_VALUE_REPORT_SIZE = 64,  /**< Wheel-value output report size in bytes. */
    PLAYSTATION_WHEEL_VALUE_FLAG = 0x01,       /**< Flag marking an asserted wheel value. */
    PLAYSTATION_WHEEL_VALUE_FLAGS_OFFSET = 1,  /**< Value-flags byte offset. */
    PLAYSTATION_WHEEL_VALUE_LOW_OFFSET = 4,    /**< Low value byte offset. */
    PLAYSTATION_WHEEL_VALUE_HIGH_OFFSET = 5,   /**< High value byte offset. */
    PLAYSTATION_WHEEL_VALUE_TIMEOUT_MS = 3000, /**< Retained value timeout in milliseconds. */
};
void usb_playstation_wheel_value_init(UsbPlaystationWheelValue *value) {
    if (value == NULL) {
        return;
    }
    *value = (UsbPlaystationWheelValue){0};
}

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

void usb_playstation_wheel_value_set_axis_copy(UsbPlaystationWheelValue *value, bool enabled) {
    if (value != NULL) {
        value->axis_copy_enabled = enabled;
    }
}

bool usb_playstation_wheel_value_copy_axes(UsbPlaystationWheelValue *value, const uint8_t axes[2]) {
    if (value == NULL || axes == NULL || !value->axis_copy_enabled) {
        return false;
    }
    value->legacy_axes[0] = axes[1];
    value->legacy_axes[1] = axes[0];
    return true;
}

bool usb_playstation_wheel_value_expire(UsbPlaystationWheelValue *value, uint32_t now_ms) {
    if (value == NULL || now_ms <= value->deadline_ms ||
        (value->legacy_axes[0] == 0 && value->legacy_axes[1] == 0)) {
        return false;
    }
    value->legacy_axes[0] = 0;
    value->legacy_axes[1] = 0;
    return true;
}

const uint8_t *usb_playstation_wheel_value_axes(const UsbPlaystationWheelValue *value) {
    return value == NULL ? NULL : value->legacy_axes;
}
