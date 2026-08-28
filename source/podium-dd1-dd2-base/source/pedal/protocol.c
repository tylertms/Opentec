#include "pedal/protocol.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_V3_PROTOCOL_RESPONSE = 0x15,
    PEDAL_V4_PROTOCOL_RESPONSE = 0x26,
    PEDAL_LEGACY_AXIS_1_COMMAND = 0x40,
    PEDAL_LEGACY_AXIS_2_COMMAND = 0x80,
    PEDAL_LEGACY_AXIS_3_COMMAND = 0xc0,
    PEDAL_LEGACY_CHANNEL_MASK = 0x3f,
};

/**
 * @brief Selects the pedal transport from the detected device and protocol response bytes.
 * @param device Device byte returned by the detection request.
 * @param response Response byte returned by the protocol request.
 * @return V3, V4, legacy, or rediscovery transport selection.
 */
PedalProtocol pedal_protocol_select(uint8_t device, uint8_t response) {
    if (device == PEDAL_DEVICE_V3 && response == PEDAL_V3_PROTOCOL_RESPONSE) {
        return PEDAL_PROTOCOL_V3;
    }
    if (device == PEDAL_DEVICE_V4 && response == PEDAL_V4_PROTOCOL_RESPONSE) {
        return PEDAL_PROTOCOL_V4;
    }
    if (device == PEDAL_DEVICE_INVALID || response == PEDAL_DEVICE_INVALID ||
        device == PEDAL_DEVICE_NONE || response == 0) {
        return PEDAL_PROTOCOL_REDISCOVER;
    }
    return PEDAL_PROTOCOL_LEGACY;
}

/**
 * @brief Builds the one-byte request for a legacy pedal channel.
 * @param channel Axis or auxiliary channel to poll.
 * @param protocol_first Configured channel bits for the second axis.
 * @param protocol_second Configured channel bits for the third axis.
 * @return Request byte sent to the pedal controller.
 */
uint8_t pedal_legacy_request(PedalLegacyChannel channel, uint8_t protocol_first,
                             uint8_t protocol_second) {
    switch (channel) {
    case PEDAL_LEGACY_AXIS_1:
        return PEDAL_LEGACY_AXIS_1_COMMAND;
    case PEDAL_LEGACY_AXIS_2:
        return (protocol_first & PEDAL_LEGACY_CHANNEL_MASK) | PEDAL_LEGACY_AXIS_2_COMMAND;
    case PEDAL_LEGACY_AXIS_3:
        return protocol_second | PEDAL_LEGACY_AXIS_3_COMMAND;
    case PEDAL_LEGACY_AUXILIARY:
    case PEDAL_LEGACY_CHANNEL_COUNT:
        return 0;
    }
    return 0;
}

/**
 * @brief Applies a one-byte legacy pedal response to its selected input channel.
 * @param channel Axis or auxiliary channel that produced the response.
 * @param response Byte received from the pedal controller.
 * @param auxiliary_locked True when another input source owns the auxiliary channel.
 * @param input Pedal input state to update.
 */
void pedal_legacy_apply_response(PedalLegacyChannel channel, uint8_t response,
                                 bool auxiliary_locked, PedalInput *input) {
    if (channel < PEDAL_LEGACY_AUXILIARY) {
        input->axes[channel] = (uint16_t)(uint8_t)~response << 8;
    } else if (channel == PEDAL_LEGACY_AUXILIARY && !auxiliary_locked) {
        input->auxiliary = response;
    }
}
