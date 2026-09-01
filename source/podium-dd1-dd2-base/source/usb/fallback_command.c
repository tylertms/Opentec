#include "usb/fallback_command.h"

#include <stddef.h>
#include <stdint.h>

/** @brief Short fallback command framing and family constants. */
enum {
    FALLBACK_COMMAND_SIZE = 7,        /**< Required short command length. */
    FALLBACK_F8_FAMILY = 0xf8,        /**< Direct tuning command family. */
    FALLBACK_F9_FAMILY = 0xf9,        /**< Cooling and security command family. */
    FALLBACK_COOLING_COMMAND = 0x02,  /**< Cooling override command code. */
    FALLBACK_SECURITY_COMMAND = 0xa0, /**< Security command code. */
    FALLBACK_SECURITY_DISABLE = 1,    /**< Security-disable parameter value. */
};

bool usb_fallback_command_decode(const UsbOutputCommand *output, UsbFallbackCommand *command) {
    if (output == NULL || command == NULL || output->kind != USB_OUTPUT_COMMAND_SHORT ||
        output->payload == NULL || output->length != FALLBACK_COMMAND_SIZE) {
        return false;
    }

    const uint8_t *payload = output->payload;
    *command = (UsbFallbackCommand){
        .value = (uint16_t)payload[2] | (uint16_t)payload[3] << 8,
        .parameters = {payload[2], payload[3], payload[4], payload[5]},
    };

    if (payload[0] == FALLBACK_F8_FAMILY) {
        switch (payload[1]) {
        case 0x02:
            command->kind = USB_FALLBACK_STEERING_RANGE_LOW;
            return true;
        case 0x03:
            command->kind = USB_FALLBACK_STEERING_RANGE_HIGH;
            return true;
        case 0x12:
            command->kind = USB_FALLBACK_DISPLAY_FLAGS;
            return true;
        case 0x15:
            command->kind = USB_FALLBACK_SENSITIVITY;
            return true;
        case 0x16:
            command->kind = USB_FALLBACK_FORCE_FEEDBACK_STRENGTH;
            return true;
        case 0x17:
            if (payload[2] < 1 || payload[2] > 2) {
                return false;
            }
            command->kind = USB_FALLBACK_FORCE_SCALE;
            return true;
        case 0x18:
            command->kind = USB_FALLBACK_NATURAL_DAMPER;
            return true;
        case 0x19:
            command->kind = USB_FALLBACK_NATURAL_FRICTION;
            return true;
        case 0x1a:
            command->kind = USB_FALLBACK_NATURAL_INERTIA;
            return true;
        case 0x1b:
            command->kind = USB_FALLBACK_INTERPOLATION;
            return true;
        case 0x1c:
            command->kind = USB_FALLBACK_FORCE_EFFECT_INTENSITY;
            return true;
        case 0x1d:
            command->kind = USB_FALLBACK_FORCE_EFFECT_STRENGTH;
            return true;
        case 0x1e:
            command->kind = USB_FALLBACK_SPRING_EFFECT_STRENGTH;
            return true;
        case 0x1f:
            command->kind = USB_FALLBACK_DAMPER_EFFECT_STRENGTH;
            return true;
        case 0x20:
            command->kind = USB_FALLBACK_VIBRATION_STRENGTH;
            return true;
        case 0x81:
            command->kind = USB_FALLBACK_STEERING_LIMIT;
            return true;
        default:
            return false;
        }
    }

    if (payload[0] != FALLBACK_F9_FAMILY) {
        return false;
    }
    if (payload[1] == FALLBACK_COOLING_COMMAND) {
        command->kind = USB_FALLBACK_COOLING_OVERRIDE;
        return true;
    }
    if (payload[1] == FALLBACK_SECURITY_COMMAND && payload[2] == FALLBACK_SECURITY_DISABLE) {
        command->kind = USB_FALLBACK_SECURITY_DISABLE;
        return true;
    }
    return false;
}
