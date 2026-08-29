#include "usb/xbox_gip_command.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    XBOX_GIP_COMMAND_PACKET = 0x0a,
    XBOX_GIP_COMMAND_GROUP_SCRIPT = 0,
    XBOX_GIP_COMMAND_SELECTOR_OFFSET = 4,
    XBOX_GIP_COMMAND_PARAMETER_OFFSET = 5,
    XBOX_GIP_COMMAND_MINIMUM_SIZE = 7,
    XBOX_GIP_COMMAND_SCRIPT_SAMPLES = 4,
    XBOX_GIP_COMMAND_SCRIPT_SLOT = 5,
    XBOX_GIP_COMMAND_SCRIPT_STATUS = 6,
    XBOX_GIP_COMMAND_SCRIPT_VALUES = 7,
    XBOX_GIP_COMMAND_SCRIPT_AXES = 8,
    XBOX_GIP_COMMAND_LAST_SAMPLE = 501,
    XBOX_GIP_COMMAND_EMPTY_SLOT = 15,
};

/**
 * @brief Reads a little-endian 16-bit command parameter.
 *
 * Combines the two parameter bytes used by Xbox GIP command packets.
 *
 * @param[in] input Two encoded parameter bytes.
 * @return Decoded unsigned parameter.
 */
static uint16_t read_u16(const uint8_t input[2]) {
    return (uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8u);
}

/**
 * @brief Decodes an Xbox GIP force-feedback script query.
 *
 * Accepts group-zero command packet 0A selectors 4 through 8. Sample queries allow first indices
 * through 501, and slot queries allow real slots through 14 plus the empty slot-15 response.
 *
 * @param[in] packet Received Xbox GIP endpoint packet.
 * @param[in] length Number of available packet bytes.
 * @param[out] command Decoded query kind and little-endian parameter.
 * @return True when the packet contains a supported script query.
 */
bool usb_xbox_gip_command_decode(const uint8_t *packet, size_t length, UsbXboxGipCommand *command) {
    if (packet == NULL || command == NULL || length < XBOX_GIP_COMMAND_MINIMUM_SIZE ||
        packet[0] != XBOX_GIP_COMMAND_PACKET || packet[1] != XBOX_GIP_COMMAND_GROUP_SCRIPT) {
        return false;
    }

    uint16_t parameter = read_u16(packet + XBOX_GIP_COMMAND_PARAMETER_OFFSET);
    switch (packet[XBOX_GIP_COMMAND_SELECTOR_OFFSET]) {
    case XBOX_GIP_COMMAND_SCRIPT_SAMPLES:
        if (parameter > XBOX_GIP_COMMAND_LAST_SAMPLE) {
            return false;
        }
        command->kind = USB_XBOX_GIP_COMMAND_SCRIPT_SAMPLES;
        break;
    case XBOX_GIP_COMMAND_SCRIPT_SLOT:
        if (parameter > XBOX_GIP_COMMAND_EMPTY_SLOT) {
            return false;
        }
        command->kind = USB_XBOX_GIP_COMMAND_SCRIPT_SLOT;
        break;
    case XBOX_GIP_COMMAND_SCRIPT_STATUS:
        command->kind = USB_XBOX_GIP_COMMAND_SCRIPT_STATUS;
        break;
    case XBOX_GIP_COMMAND_SCRIPT_VALUES:
        command->kind = USB_XBOX_GIP_COMMAND_SCRIPT_VALUES;
        break;
    case XBOX_GIP_COMMAND_SCRIPT_AXES:
        command->kind = USB_XBOX_GIP_COMMAND_SCRIPT_AXES;
        break;
    default:
        return false;
    }
    command->parameter = parameter;
    return true;
}
