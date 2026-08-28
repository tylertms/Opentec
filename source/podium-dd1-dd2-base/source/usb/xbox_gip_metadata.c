#include "usb/xbox_gip_metadata.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint8_t payload_size;
} XboxGipMessageDefinition;

static size_t append_bytes(uint8_t *output, size_t offset, const uint8_t *data, size_t length) {
    for (size_t index = 0; index < length; index++) {
        output[offset + index] = data[index];
    }
    return offset + length;
}

static size_t append_string(uint8_t *output, size_t offset, const char *text, uint16_t length) {
    output[offset++] = (uint8_t)length;
    output[offset++] = (uint8_t)(length >> 8);
    return append_bytes(output, offset, (const uint8_t *)text, length);
}

static size_t append_message(uint8_t *output, size_t offset,
                             const XboxGipMessageDefinition *message) {
    output[offset++] = 0x17;
    output[offset++] = 0;
    output[offset++] = message->type;
    output[offset++] = message->flags;
    output[offset++] = 0;
    output[offset++] = 1;
    output[offset++] = 0;
    output[offset++] = message->payload_size;
    for (uint8_t index = 8; index < 23; index++) {
        output[offset++] = 0;
    }
    return offset;
}

/**
 * @brief Encodes the Xbox GIP wheel metadata document.
 *
 * Emits the wheel and navigation interface names, capability identifiers, and ten supported GIP
 * message declarations used by the Xbox feature exchange.
 *
 * @param[out] output Destination for the 449-byte metadata document.
 */
void usb_xbox_gip_metadata_encode(uint8_t output[USB_XBOX_GIP_METADATA_SIZE]) {
    static const uint8_t header[] = {
        0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc1,
        0x01, 0xca, 0x00, 0x16, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x23, 0x00, 0x29, 0x00, 0x89, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x09, 0x00, 0x00, 0x06,
        0x01, 0x02, 0x03, 0x04, 0x06, 0x07, 0x05, 0x01, 0x04, 0x05, 0x06, 0x0a, 0x03,
    };
    static const uint8_t capabilities[] = {
        0x04, 0xcf, 0x79, 0x69, 0x64, 0x71, 0x6b, 0x96, 0x4e, 0x8d, 0xf9, 0x59, 0xe3, 0x98,
        0xd7, 0x42, 0x0c, 0xe7, 0x1f, 0xf3, 0xb8, 0x86, 0x73, 0xe9, 0x40, 0xa9, 0xf8, 0x2f,
        0x21, 0x26, 0x3a, 0xcf, 0xb7, 0x56, 0xff, 0x76, 0x97, 0xfd, 0x9b, 0x81, 0x45, 0xad,
        0x45, 0xb6, 0x45, 0xbb, 0xa5, 0x26, 0xd6, 0xfe, 0xd2, 0xdd, 0xec, 0x87, 0xd3, 0x94,
        0x42, 0xbd, 0x96, 0x1a, 0x71, 0x2e, 0x3d, 0xc7, 0x7d, 0x0a,
    };
    static const XboxGipMessageDefinition messages[] = {
        {.type = 0x20, .flags = 0x32, .payload_size = 0x10},
        {.type = 0x21, .flags = 0x33, .payload_size = 0x10},
        {.type = 0x25, .flags = 0x31, .payload_size = 0x10},
        {.type = 0x11, .flags = 0x0d, .payload_size = 0x10},
        {.type = 0x0a, .flags = 0x03, .payload_size = 0x08},
        {.type = 0x0b, .flags = 0x3c, .payload_size = 0x08},
        {.type = 0x0c, .flags = 0x09, .payload_size = 0x08},
        {.type = 0x0d, .flags = 0x35, .payload_size = 0x08},
        {.type = 0x0e, .flags = 0x38, .payload_size = 0x08},
        {.type = 0x0f, .flags = 0x3c, .payload_size = 0x08},
    };

    size_t offset = append_bytes(output, 0, header, sizeof(header));
    offset = append_string(output, offset, "Microsoft.Xbox.Input.Wheel", 26);
    offset = append_string(output, offset, "Windows.Xbox.Input.Wheel", 24);
    offset = append_string(output, offset, "Windows.Xbox.Input.NavigationController", 39);
    offset = append_bytes(output, offset, capabilities, sizeof(capabilities));
    for (size_t index = 0; index < sizeof(messages) / sizeof(messages[0]); index++) {
        offset = append_message(output, offset, &messages[index]);
    }
}
