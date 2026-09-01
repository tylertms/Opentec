#ifndef OPENTEC_BASE_USB_UPDATER_PROTOCOL_H
#define OPENTEC_BASE_USB_UPDATER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Updater protocol sizes and framing constants. */
enum {
    USB_UPDATER_DEVICE_IDENTITY_SIZE =
        4, /**< Number of bytes in a runtime identity returned by the updater protocol. */
    USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE =
        6, /**< Number of bytes in an updater device-information response. */
};

/** @brief Operation selected by one motor-updater USB packet. */
typedef enum {
    USB_UPDATER_REQUEST_NONE,        /**< No supported operation was selected. */
    USB_UPDATER_REQUEST_BRIDGE,      /**< A marker-prefixed request must be bridged to the attached
                                        device. */
    USB_UPDATER_REQUEST_DEVICE_INFO, /**< A device-information response must be returned. */
    USB_UPDATER_REQUEST_RESET, /**< The guarded reset request must be exposed to the runtime owner.
                                */
} UsbUpdaterRequestKind;

/** @brief Decoded updater operation and retained packet view. */
typedef struct {
    const uint8_t *data;        /**< Original request bytes for a bridge operation. */
    uint8_t length;             /**< Number of bytes in #data. */
    UsbUpdaterRequestKind kind; /**< Operation selected by the request. */
} UsbUpdaterRequest;

/**
 * @brief Decodes one motor-updater USB request.
 *
 * Selects marker 0x5A bridge packets, the F8/01 device-information query, or the guarded
 * F8/09/01/FE reset operation. Unsupported and truncated packets are rejected.
 *
 * @param[in] data Complete updater packet bytes.
 * @param[in] length Number of bytes in @p data, from one through 63.
 * @param[out] request Decoded operation and packet view.
 * @return `true` when the packet selects a supported updater operation; otherwise `false`.
 */
bool usb_updater_protocol_decode(const uint8_t *data, uint8_t length, UsbUpdaterRequest *request);

/**
 * @brief Encodes the motor-updater device-information response.
 *
 * Prefixes the selected four-character runtime identity with the F8/01 control response header.
 *
 * @param[in] identity Four-character runtime identity.
 * @param[out] response Six-byte updater response.
 */
void usb_updater_protocol_encode_device_info(
    const uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE],
    uint8_t response[USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE]);

#endif
