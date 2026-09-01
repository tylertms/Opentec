#ifndef OPENTEC_BASE_USB_UPDATER_IDENTITY_H
#define OPENTEC_BASE_USB_UPDATER_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/operating_mode_command.h"
#include "usb/updater_protocol.h"

/** @brief Updater identity-selection constants. */
enum {
    USB_UPDATER_IDENTITY_AUTOMATIC =
        0xff /**< Select identity from the active runtime and probe response. */
};

/** @brief Runtime and attached-wheel state used to select an updater identity. */
typedef struct {
    UsbRuntimeMode runtime_mode; /**< Runtime route currently exposing the updater interface. */
    BoardVariant board_variant;  /**< Base hardware variant. */
    uint8_t wheel_mode;          /**< Attached-wheel protocol mode. */
    uint8_t response_selector;   /**< Probe-selected identity selector, or
                                    #USB_UPDATER_IDENTITY_AUTOMATIC. */
    bool adapter_connected;      /**< Whether the attached wheel is connected through an adapter. */
} UsbUpdaterIdentityInput;

/**
 * @brief Selects an identity selector from an updater probe response.
 *
 * Maps supported low and high command families for USB bridge and protocol recovery routes,
 * maps protocol bridge command values, and leaves other routes on automatic selection.
 *
 * @param[in] runtime_mode Active updater bridge route.
 * @param[in] command Command byte returned by the attached device.
 * @return Identity selector for later device-information responses.
 */
uint8_t usb_updater_identity_selector(UsbRuntimeMode runtime_mode, uint8_t command);

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
                                 uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE]);

#endif
