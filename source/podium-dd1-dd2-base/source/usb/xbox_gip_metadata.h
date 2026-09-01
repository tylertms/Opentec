#ifndef OPENTEC_BASE_USB_XBOX_GIP_METADATA_H
#define OPENTEC_BASE_USB_XBOX_GIP_METADATA_H

#include <stdint.h>

/** @brief Xbox GIP metadata constants. */
enum {
    USB_XBOX_GIP_METADATA_SIZE = 449 /**< Number of bytes in the encoded metadata document. */
};

/**
 * @brief Encodes the Xbox GIP wheel metadata document.
 *
 * Emits the wheel and navigation interface names, capability identifiers, and supported GIP
 * message declarations used by the Xbox feature exchange.
 *
 * @param[out] output Destination for the 449-byte metadata document.
 */
void usb_xbox_gip_metadata_encode(uint8_t output[USB_XBOX_GIP_METADATA_SIZE]);

#endif
