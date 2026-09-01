#include "pedal/protocol_command.h"

#include <stdbool.h>
#include <stddef.h>

#include "usb/operating_mode_command.h"

/**
 * @brief Operating-mode selectors for pedal protocol commands.
 */
enum {
    PEDAL_DEVICE_CONTROL_OPCODE = 1,          /**< Device-control operating-mode opcode. */
    PEDAL_PROTOCOL_UPDATE_SELECTOR = 4,       /**< Selector for a protocol update. */
    PEDAL_PROTOCOL_LEGACY_SCALE_SELECTOR = 8, /**< Selector for a legacy scale update. */
};

/**
 * @brief Decodes pedal protocol updates from the operating-mode envelope.
 *
 * Accepts device-control selector four as a three-byte protocol update and selector eight as a
 * one-byte legacy scale update.
 *
 * @param[in] source Decoded F8 09 operating-mode command.
 * @param[out] command Pedal protocol operation and values.
 * @return True when the opcode and selector identify a supported pedal protocol command.
 */
bool pedal_protocol_command_decode(const UsbOperatingModeCommand *source,
                                   PedalProtocolCommand *command) {
    if (source == NULL || command == NULL || source->opcode != PEDAL_DEVICE_CONTROL_OPCODE) {
        return false;
    }

    if (source->parameters[0] == PEDAL_PROTOCOL_UPDATE_SELECTOR) {
        *command = (PedalProtocolCommand){
            .kind = PEDAL_PROTOCOL_COMMAND_UPDATE,
            .value = source->parameters[1],
            .first = source->parameters[2],
            .second = source->parameters[3],
        };
        return true;
    }
    if (source->parameters[0] == PEDAL_PROTOCOL_LEGACY_SCALE_SELECTOR) {
        *command = (PedalProtocolCommand){
            .kind = PEDAL_PROTOCOL_COMMAND_LEGACY_SCALE,
            .value = source->parameters[1],
        };
        return true;
    }
    return false;
}
