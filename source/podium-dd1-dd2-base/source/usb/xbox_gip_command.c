#include "usb/xbox_gip_command.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Private packet values and limits used by Xbox GIP command decoding. */
enum {
    XBOX_GIP_COMMAND_PACKET = 0x0a,               /**< Command packet identifier. */
    XBOX_GIP_COMMAND_GROUP_SCRIPT = 0,            /**< Group value for script commands. */
    XBOX_GIP_COMMAND_GROUP_CONTROL = 0x20,        /**< Group value for host-control commands. */
    XBOX_GIP_COMMAND_SELECTOR_OFFSET = 4,         /**< Selector byte offset in a command packet. */
    XBOX_GIP_COMMAND_PARAMETER_OFFSET = 5,        /**< Parameter byte offset in a command packet. */
    XBOX_GIP_COMMAND_MINIMUM_SIZE = 7,            /**< Minimum command packet length. */
    XBOX_GIP_COMMAND_CAPABILITIES = 0,            /**< Capabilities selector. */
    XBOX_GIP_COMMAND_STEERING_RANGE = 1,          /**< Steering-range selector. */
    XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH = 2, /**< Force-feedback-strength selector. */
    XBOX_GIP_COMMAND_TRANSFER_STATUS = 3,         /**< Transfer-status selector. */
    XBOX_GIP_COMMAND_CONTROL_HOST_CAPABILITY = 0, /**< Host-capability selector in group 0x20. */
    XBOX_GIP_COMMAND_CONTROL_TRANSFER_STATUS = 1, /**< Transfer-status selector in group 0x20. */
    XBOX_GIP_COMMAND_SCRIPT_SAMPLES = 4,          /**< Script sample selector. */
    XBOX_GIP_COMMAND_SCRIPT_SLOT = 5,             /**< Script slot selector. */
    XBOX_GIP_COMMAND_SCRIPT_STATUS = 6,           /**< Script status selector. */
    XBOX_GIP_COMMAND_SCRIPT_VALUES = 7,           /**< Script values selector. */
    XBOX_GIP_COMMAND_SCRIPT_AXES = 8,             /**< Script axes selector. */
    XBOX_GIP_COMMAND_EXTENDED_STATUS = 9,         /**< Extended-status selector. */
    XBOX_GIP_COMMAND_LAST_SAMPLE = 501,           /**< Largest accepted script sample index. */
    XBOX_GIP_COMMAND_EMPTY_SLOT = 15,             /**< Largest accepted script slot index. */
    XBOX_GIP_STEERING_RANGE_MINIMUM_DEGREES = 90, /**< Minimum normalized steering range. */
    XBOX_GIP_STEERING_RANGE_MAXIMUM_DEGREES = 1080, /**< Maximum normalized steering range. */
    XBOX_GIP_STEERING_RANGE_STEP_DEGREES = 10,      /**< Normalized steering-range step. */
    XBOX_GIP_FORCE_FEEDBACK_LEVEL_MAXIMUM = 255,    /**< Maximum host force-feedback level. */
    XBOX_GIP_FORCE_FEEDBACK_PERCENT_MAXIMUM = 100, /**< Maximum stored force-feedback percentage. */
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

bool usb_xbox_gip_command_decode(const uint8_t *packet, size_t length, UsbXboxGipCommand *command) {
    if (packet == NULL || command == NULL || length < XBOX_GIP_COMMAND_MINIMUM_SIZE ||
        packet[0] != XBOX_GIP_COMMAND_PACKET) {
        return false;
    }

    uint16_t parameter = read_u16(packet + XBOX_GIP_COMMAND_PARAMETER_OFFSET);
    if (packet[1] == XBOX_GIP_COMMAND_GROUP_CONTROL) {
        if (packet[XBOX_GIP_COMMAND_SELECTOR_OFFSET] == XBOX_GIP_COMMAND_CONTROL_HOST_CAPABILITY) {
            command->kind = USB_XBOX_GIP_COMMAND_HOST_CAPABILITY;
            command->parameter = packet[XBOX_GIP_COMMAND_PARAMETER_OFFSET];
            return true;
        }
        if (packet[XBOX_GIP_COMMAND_SELECTOR_OFFSET] != XBOX_GIP_COMMAND_CONTROL_TRANSFER_STATUS) {
            return false;
        }
        command->kind = USB_XBOX_GIP_COMMAND_TRANSFER_STATUS;
        command->parameter = parameter;
        return true;
    }
    if (packet[1] != XBOX_GIP_COMMAND_GROUP_SCRIPT) {
        return false;
    }

    switch (packet[XBOX_GIP_COMMAND_SELECTOR_OFFSET]) {
    case XBOX_GIP_COMMAND_CAPABILITIES:
        command->kind = USB_XBOX_GIP_COMMAND_CAPABILITIES;
        break;
    case XBOX_GIP_COMMAND_STEERING_RANGE:
        command->kind = USB_XBOX_GIP_COMMAND_STEERING_RANGE;
        break;
    case XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH:
        command->kind = USB_XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH;
        parameter = packet[XBOX_GIP_COMMAND_PARAMETER_OFFSET];
        break;
    case XBOX_GIP_COMMAND_TRANSFER_STATUS:
        command->kind = USB_XBOX_GIP_COMMAND_TRANSFER_STATUS;
        break;
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
    case XBOX_GIP_COMMAND_EXTENDED_STATUS:
        command->kind = USB_XBOX_GIP_COMMAND_EXTENDED_STATUS;
        break;
    default:
        return false;
    }
    command->parameter = parameter;
    return true;
}

uint16_t usb_xbox_gip_steering_range_normalize(uint16_t requested_degrees) {
    requested_degrees /= 10u;
    if (requested_degrees < XBOX_GIP_STEERING_RANGE_MINIMUM_DEGREES) {
        return XBOX_GIP_STEERING_RANGE_MINIMUM_DEGREES;
    }
    if (requested_degrees > XBOX_GIP_STEERING_RANGE_MAXIMUM_DEGREES) {
        return XBOX_GIP_STEERING_RANGE_MAXIMUM_DEGREES;
    }
    return requested_degrees / XBOX_GIP_STEERING_RANGE_STEP_DEGREES *
           XBOX_GIP_STEERING_RANGE_STEP_DEGREES;
}

uint8_t usb_xbox_gip_force_feedback_strength_normalize(uint8_t requested_level) {
    return (uint8_t)((uint16_t)requested_level * XBOX_GIP_FORCE_FEEDBACK_PERCENT_MAXIMUM /
                     XBOX_GIP_FORCE_FEEDBACK_LEVEL_MAXIMUM);
}
