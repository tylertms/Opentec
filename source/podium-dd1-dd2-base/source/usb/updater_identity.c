#include "usb/updater_identity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "usb/operating_mode_command.h"
#include "usb/updater_protocol.h"

enum {
    USB_UPDATER_HIGH_SELECTOR = 0x80,
    USB_UPDATER_SELECTOR_INDEX = 0x7f,
    USB_UPDATER_USB_FALLBACK_INDEX = 5,
    USB_UPDATER_USB_DIRECT_INDEX = 6,
    USB_UPDATER_USB_ADAPTER_INDEX = 7,
    USB_UPDATER_HIGH_DEFAULT_INDEX = 20,
    USB_UPDATER_PULSE_WHEEL_MODE = 0x1b,
};

static const uint8_t low_identities[][USB_UPDATER_DEVICE_IDENTITY_SIZE + 1] = {
    "FFFF", "kcfg", "dd10", "pdqr", "r650", "rfor", "wmcl", "phub", "pbpe",
    "FFFF", "FFFF", "wgts", "chub", "wwrc", "w918", "wbmw", "xhub", "wf1e",
    "FFFF", "FFFF", "FFFF", "FFFF", "FFFF", "FFFF", "wgt3", "wgt3",
};

static const uint8_t high_identities[][USB_UPDATER_DEVICE_IDENTITY_SIZE + 1] = {
    "FFFF", "FFFF", "FFFF", "FFFF", "FFFF", "zfor", "zmcl", "FFFF", "FFFF", "FFFF", "FFFF",
    "zgts", "FFFF", "FFFF", "FFFF", "FFFF", "zhub", "FFFF", "zgtx", "zbm4", "zpbr", "zpvg",
    "zbgt", "zuhx", "zsrs", "FFFF", "zfss", "FFFF", "zfv3", "FFFF", "zfss",
};

/**
 * @brief Copies one bounded updater identity table entry.
 *
 * Uses table entry zero when the requested index is outside the selected identity family.
 *
 * @param[in] table Identity family table.
 * @param[in] count Number of entries in the table.
 * @param[in] index Requested entry index.
 * @param[out] identity Selected four-character identity.
 */
static void select_entry(const uint8_t table[][USB_UPDATER_DEVICE_IDENTITY_SIZE + 1], uint8_t count,
                         uint8_t index, uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE]) {
    if (index >= count) {
        index = 0;
    }
    memcpy(identity, table[index], USB_UPDATER_DEVICE_IDENTITY_SIZE);
}

/**
 * @brief Selects an explicit updater response identity.
 *
 * Routes selectors with bit seven set to the high identity family and all other selectors to the
 * low family. Automatic selection uses the caller-provided default index.
 *
 * @param[in] selector Explicit response selector or 0xFF for automatic selection.
 * @param[in] automatic_high True when automatic selection uses the high identity family.
 * @param[in] automatic_index Default table index for automatic selection.
 * @param[out] identity Selected four-character identity.
 */
static void select_response(uint8_t selector, bool automatic_high, uint8_t automatic_index,
                            uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE]) {
    bool high = selector == USB_UPDATER_IDENTITY_AUTOMATIC
                    ? automatic_high
                    : (selector & USB_UPDATER_HIGH_SELECTOR) != 0;
    uint8_t index = selector == USB_UPDATER_IDENTITY_AUTOMATIC
                        ? automatic_index
                        : selector & USB_UPDATER_SELECTOR_INDEX;
    if (high) {
        select_entry(high_identities, (uint8_t)(sizeof(high_identities) / sizeof(*high_identities)),
                     index, identity);
    } else {
        select_entry(low_identities, (uint8_t)(sizeof(low_identities) / sizeof(*low_identities)),
                     index, identity);
    }
}

/**
 * @brief Tests whether a wheel mode maps directly into the low identity table.
 *
 * Accepts modes 9 through 18 and 28 through 30, whose identity index is the wheel mode minus five.
 *
 * @param[in] wheel_mode Attached-wheel mode.
 * @return True when the wheel mode has a direct low-table identity; otherwise false.
 */
static bool direct_wheel_identity(uint8_t wheel_mode) {
    return (wheel_mode >= 9 && wheel_mode <= 0x12) || (wheel_mode >= 0x1c && wheel_mode <= 0x1e);
}

/**
 * @brief Selects the updater device identity for the active bridge path.
 *
 * Maps auxiliary and status runtimes directly, derives USB bridge identities from attached-wheel
 * mode and adapter state, and uses the high protocol default for protocol bridge mode. Explicit
 * response selectors override the automatic wheel-derived choice where supported.
 *
 * @param[in] input Current runtime, wheel mode, selector, and adapter state.
 * @param[out] identity Selected four-character updater identity.
 */
void usb_updater_identity_select(const UsbUpdaterIdentityInput *input,
                                 uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE]) {
    if (input == NULL || identity == NULL) {
        return;
    }
    if (input->runtime_mode == USB_RUNTIME_MODE_PROTOCOL_BRIDGE) {
        select_response(input->response_selector, true, USB_UPDATER_HIGH_DEFAULT_INDEX, identity);
        return;
    }
    if (input->runtime_mode != USB_RUNTIME_MODE_USB_BRIDGE) {
        select_entry(low_identities, (uint8_t)(sizeof(low_identities) / sizeof(*low_identities)),
                     (uint8_t)input->runtime_mode, identity);
        return;
    }
    if (input->wheel_mode == 4 || input->wheel_mode == 6) {
        select_entry(low_identities, (uint8_t)(sizeof(low_identities) / sizeof(*low_identities)),
                     input->adapter_connected ? USB_UPDATER_USB_ADAPTER_INDEX
                                              : USB_UPDATER_USB_DIRECT_INDEX,
                     identity);
        return;
    }
    if (direct_wheel_identity(input->wheel_mode)) {
        select_entry(low_identities, (uint8_t)(sizeof(low_identities) / sizeof(*low_identities)),
                     input->wheel_mode - 5, identity);
        return;
    }
    if (input->wheel_mode == USB_UPDATER_PULSE_WHEEL_MODE) {
        select_response(input->response_selector, true, USB_UPDATER_HIGH_DEFAULT_INDEX, identity);
        return;
    }
    select_response(input->response_selector, false, USB_UPDATER_USB_FALLBACK_INDEX, identity);
}
