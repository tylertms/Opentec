#include "usb/xbox_gip_response.h"

#include <stdint.h>
#include <string.h>

#include "usb/console_descriptor.h"

enum {
    XBOX_GIP_DIGEST_RESPONSE = 2,
    XBOX_GIP_SEQUENCE_SUBCOMMAND = 0x20,
    XBOX_GIP_DIGEST_PAYLOAD_SIZE = 0x1c,
    XBOX_GIP_EXTENDED_STATUS_WHEEL_MODE = 0x1d,
};

/**
 * @brief Takes the next Xbox GIP response sequence.
 *
 * Returns the current sequence and advances it, except that value 255 is replaced with 1 and
 * stored as the next value.
 *
 * @param[in,out] next_sequence Sequence state to consume and advance.
 * @return Sequence value for the current response.
 */
uint8_t usb_xbox_gip_sequence_take(uint8_t *next_sequence) {
    uint8_t sequence = *next_sequence;
    if (sequence == UINT8_MAX) {
        *next_sequence = 1;
        return 1;
    }

    *next_sequence = sequence + 1;
    return sequence;
}

/**
 * @brief Encodes the Xbox GIP discovery digest response.
 *
 * Emits the 32-byte discovery identity, including the eight-byte device digest, base and wheel
 * mode code, protocol version, status version, and three feature versions.
 *
 * @param[in] variant DD1 or DD2 hardware variant.
 * @param[in] wheel_mode Attached wheel mode used for the mode and status versions.
 * @param[in] sequence Response sequence value.
 * @param[in] digest Eight-byte device digest.
 * @param[out] output Destination for the digest response.
 */
void usb_xbox_gip_digest_response_encode(BoardVariant variant, uint8_t wheel_mode, uint8_t sequence,
                                         const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                         uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_DIGEST_RESPONSE_SIZE);
    output[0] = XBOX_GIP_DIGEST_RESPONSE;
    output[1] = XBOX_GIP_SEQUENCE_SUBCOMMAND;
    output[2] = sequence;
    output[3] = XBOX_GIP_DIGEST_PAYLOAD_SIZE;
    memcpy(&output[4], digest, USB_XBOX_GIP_DIGEST_SIZE);
    output[12] = 0xb7;
    output[13] = 0x0e;

    uint8_t mode_code = usb_xbox_gip_mode_code(variant, wheel_mode);
    if (mode_code != 0) {
        output[14] = mode_code;
        output[15] = 0x0f;
    }

    static const uint8_t protocol_version[] = {3, 0, 9, 0, 0, 0, 1, 0};
    memcpy(&output[16], protocol_version, sizeof(protocol_version));
    output[24] = wheel_mode == XBOX_GIP_EXTENDED_STATUS_WHEEL_MODE ? 5 : 4;
    output[26] = 1;
    output[28] = 1;
    output[30] = 1;
}
