#include "usb/xbox_gip_response.h"

#include <stdint.h>
#include <string.h>

#include "usb/console_descriptor.h"

enum {
    XBOX_GIP_DIGEST_RESPONSE = 2,
    XBOX_GIP_TRANSFER_STATUS_RESPONSE = 1,
    XBOX_GIP_CAPABILITY_RESPONSE = 0x21,
    XBOX_GIP_READY_RESPONSE = 3,
    XBOX_GIP_INPUT_RESPONSE = 0x20,
    XBOX_GIP_SEQUENCE_SUBCOMMAND = 0x20,
    XBOX_GIP_DIGEST_PAYLOAD_SIZE = 0x1c,
    XBOX_GIP_READY_PAYLOAD_SIZE = 4,
    XBOX_GIP_TRANSFER_STATUS_PAYLOAD_SIZE = 9,
    XBOX_GIP_TRANSFER_STATUS_ERROR = 2,
    XBOX_GIP_CAPABILITY_PAYLOAD_SIZE = 0x33,
    XBOX_GIP_CAPABILITY_CLASS = 0x10,
    XBOX_GIP_CAPABILITY_AXIS_COUNT = 8,
    XBOX_GIP_CAPABILITY_RANGE_UPPER = 0x5a,
    XBOX_GIP_CAPABILITY_RANGE_LOWER = 0x38,
    XBOX_GIP_CAPABILITY_PEDAL_COUNT = 4,
    XBOX_GIP_CAPABILITY_FLAGS = 0x48,
    XBOX_GIP_EXTENDED_STATUS_WHEEL_MODE = 0x1d,
    XBOX_GIP_INPUT_PAYLOAD_SIZE = 0x32,
    XBOX_GIP_INPUT_AXIS_MODE = 0x66,
    XBOX_GIP_INPUT_AXIS_FLAGS = 0x08,
    XBOX_GIP_INPUT_AUXILIARY_AXIS_FLAG = 1 << 4,
    XBOX_GIP_INPUT_THIRD_PEDAL_FLAG = 1 << 5,
    XBOX_GIP_INPUT_SECOND_PEDAL_FLAG = 1 << 6,
    XBOX_GIP_INPUT_FIRST_PEDAL_FLAG = 1 << 7,
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

/**
 * @brief Encodes the Xbox GIP ready response.
 *
 * Emits the eight-byte response returned after accepted activation, pause, suspend, and reset
 * session commands.
 *
 * @param[in] sequence Response sequence value.
 * @param[out] output Destination for the ready response.
 */
void usb_xbox_gip_ready_response_encode(uint8_t sequence,
                                        uint8_t output[USB_XBOX_GIP_READY_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_READY_RESPONSE_SIZE);
    output[0] = XBOX_GIP_READY_RESPONSE;
    output[1] = XBOX_GIP_SEQUENCE_SUBCOMMAND;
    output[2] = sequence;
    output[3] = XBOX_GIP_READY_PAYLOAD_SIZE;
    output[4] = 0x80;
    output[5] = 1;
}

/**
 * @brief Encodes the Xbox GIP transfer-status response.
 *
 * Echoes the first two request bytes and reports transfer status 2 with zero transferred and
 * remaining counts.
 *
 * @param[in] sequence Current response sequence value without advancing it.
 * @param[in] request First two bytes of the triggering session request.
 * @param[out] output Destination for the transfer-status response.
 */
void usb_xbox_gip_transfer_status_response_encode(
    uint8_t sequence, const uint8_t request[2],
    uint8_t output[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE);
    output[0] = XBOX_GIP_TRANSFER_STATUS_RESPONSE;
    output[1] = XBOX_GIP_SEQUENCE_SUBCOMMAND;
    output[2] = sequence;
    output[3] = XBOX_GIP_TRANSFER_STATUS_PAYLOAD_SIZE;
    output[4] = XBOX_GIP_TRANSFER_STATUS_ERROR;
    output[5] = request[0];
    output[6] = request[1];
}

/**
 * @brief Encodes the Xbox GIP wheel capability response.
 *
 * Emits the 51-byte capability payload describing the wheel class, eight axes, fixed range fields,
 * four pedals, and capability flags. Reserved payload bytes are cleared.
 *
 * @param[in] sequence Response sequence value.
 * @param[out] output Destination for the capability response.
 */
void usb_xbox_gip_capability_response_encode(
    uint8_t sequence, uint8_t output[USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE);
    output[0] = XBOX_GIP_CAPABILITY_RESPONSE;
    output[2] = sequence;
    output[3] = XBOX_GIP_CAPABILITY_PAYLOAD_SIZE;
    output[4] = XBOX_GIP_CAPABILITY_CLASS;
    output[5] = XBOX_GIP_CAPABILITY_CLASS;
    output[6] = XBOX_GIP_CAPABILITY_CLASS;
    output[7] = XBOX_GIP_CAPABILITY_CLASS;
    output[8] = XBOX_GIP_CAPABILITY_AXIS_COUNT;
    output[9] = XBOX_GIP_CAPABILITY_RANGE_UPPER;
    output[11] = XBOX_GIP_CAPABILITY_RANGE_LOWER;
    output[12] = XBOX_GIP_CAPABILITY_PEDAL_COUNT;
    output[13] = 1;
    output[14] = XBOX_GIP_CAPABILITY_FLAGS;
}

/**
 * @brief Encodes the Xbox GIP input-state response.
 *
 * Emits the 50-byte controller payload with buttons, steering, pedals, wheel controls, active-axis
 * flags, auxiliary buttons, and the fixed extension marker. The remaining reserved payload bytes
 * are cleared.
 *
 * @param[in] sequence Response sequence value.
 * @param[in] snapshot Logical controller state to encode.
 * @param[out] output Destination for the input-state response.
 */
void usb_xbox_gip_input_response_encode(uint8_t sequence, const UsbXboxGipInputSnapshot *snapshot,
                                        uint8_t output[USB_XBOX_GIP_INPUT_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_INPUT_RESPONSE_SIZE);
    output[0] = XBOX_GIP_INPUT_RESPONSE;
    output[2] = sequence;
    output[3] = XBOX_GIP_INPUT_PAYLOAD_SIZE;
    memcpy(&output[4], snapshot->buttons, sizeof(snapshot->buttons));
    output[6] = (uint8_t)snapshot->steering;
    output[7] = (uint8_t)(snapshot->steering >> 8);
    for (uint8_t axis = 0; axis < USB_XBOX_GIP_INPUT_PEDAL_COUNT; axis++) {
        output[8 + axis * 2] = (uint8_t)snapshot->pedals[axis];
        output[9 + axis * 2] = (uint8_t)(snapshot->pedals[axis] >> 8);
    }
    output[14] = snapshot->auxiliary_pedal;
    output[15] = snapshot->axis_mode == 1 ? XBOX_GIP_INPUT_AXIS_MODE : 0;
    output[16] = snapshot->led_state;
    uint16_t steering_range_tenths = snapshot->steering_range_degrees * 10u;
    output[17] = (uint8_t)steering_range_tenths;
    output[18] = (uint8_t)(steering_range_tenths >> 8);
    output[19] = snapshot->force_feedback_level;
    output[20] = XBOX_GIP_INPUT_AXIS_FLAGS |
                 (snapshot->auxiliary_pedal_active ? XBOX_GIP_INPUT_AUXILIARY_AXIS_FLAG : 0) |
                 (snapshot->pedal_active[2] ? XBOX_GIP_INPUT_THIRD_PEDAL_FLAG : 0) |
                 (snapshot->pedal_active[1] ? XBOX_GIP_INPUT_SECOND_PEDAL_FLAG : 0) |
                 (snapshot->pedal_active[0] ? XBOX_GIP_INPUT_FIRST_PEDAL_FLAG : 0);
    memcpy(&output[21], snapshot->clutch_paddles, sizeof(snapshot->clutch_paddles));
    memcpy(&output[23], snapshot->selectors, sizeof(snapshot->selectors));
    output[29] = snapshot->button_flags;
    output[30] = snapshot->packed_buttons;
    memcpy(&output[33], snapshot->auxiliary_buttons, sizeof(snapshot->auxiliary_buttons));
    output[36] = UINT8_MAX;
    output[37] = snapshot->extended_button;
}
