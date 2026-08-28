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
    PEDAL_V3_HANDSHAKE_FRAME = 2,
    PEDAL_V3_STATUS_FRAME = 0,
    PEDAL_V3_INPUT_COMMAND_FRAME = 3,
    PEDAL_V3_CONFIGURATION_FRAME = 6,
    PEDAL_V3_KEEPALIVE_FRAME = 0x10,
    PEDAL_V3_NORMAL_BRAKE_STEP = 10,
    PEDAL_V3_CALIBRATION_BRAKE_STEP = 5,
    PEDAL_V3_DIRECT_CONTROL_MASK = 0x0f,
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

/**
 * @brief Builds the V3 startup or recovery handshake frame.
 * @param recovering True for the recovery handshake sent after a stream failure.
 * @param frame Destination for the message type and payload.
 */
void pedal_v3_build_handshake(bool recovering, PedalFrame *frame) {
    *frame = (PedalFrame){
        .type = PEDAL_V3_HANDSHAKE_FRAME,
        .payload = {recovering ? 0 : UINT8_MAX, recovering ? UINT8_MAX : 0},
    };
}

/**
 * @brief Builds the V3 protocol-status frame.
 * @param status Protocol value, channel selectors, and scale to publish.
 * @param frame Destination for the message type and payload.
 */
void pedal_v3_build_status(const PedalProtocolStatus *status, PedalFrame *frame) {
    *frame = (PedalFrame){
        .type = PEDAL_V3_STATUS_FRAME,
        .payload = {status->value, status->first, status->second, status->scale},
    };
}

/**
 * @brief Builds the next pending V3 pedal-control frame.
 * @param pending Bit mask of pending up, down, enable, disable, and automatic commands.
 * @param frame Destination for the next control frame.
 * @return Commands that remain pending after the frame's commands are consumed.
 */
uint8_t pedal_v3_build_control(uint8_t pending, PedalFrame *frame) {
    uint8_t direct = pending & PEDAL_V3_DIRECT_CONTROL_MASK;
    if (direct != 0) {
        *frame = (PedalFrame){
            .type = PEDAL_V3_HANDSHAKE_FRAME,
            .payload = {UINT8_MAX, 0, (direct & PEDAL_V3_CONTROL_UP) != 0 ? UINT8_MAX : 0,
                        (direct & PEDAL_V3_CONTROL_DOWN) != 0 ? UINT8_MAX : 0,
                        (direct & PEDAL_V3_CONTROL_ENABLE) != 0 ? UINT8_MAX : 0,
                        (direct & PEDAL_V3_CONTROL_DISABLE) != 0 ? UINT8_MAX : 0},
        };
        return pending & (uint8_t)~direct;
    }

    *frame = (PedalFrame){
        .type = PEDAL_V3_HANDSHAKE_FRAME,
        .payload = {UINT8_MAX, 0, 0, 0, 0, UINT8_MAX},
    };
    return pending & (uint8_t)~PEDAL_V3_CONTROL_AUTOMATIC;
}

/**
 * @brief Builds a V3 three-channel input command.
 * @param values Three command bytes forwarded to the pedal controller.
 * @param frame Destination for the input-command frame.
 */
void pedal_v3_build_input_command(const uint8_t values[PEDAL_INPUT_AXIS_COUNT], PedalFrame *frame) {
    *frame = (PedalFrame){
        .type = PEDAL_V3_INPUT_COMMAND_FRAME,
        .payload = {values[0], values[1], values[2]},
    };
}

/**
 * @brief Builds a V3 brake-force configuration frame.
 * @param brake_force Configured alternate brake force.
 * @param fine_scale True for five-point calibration steps; false for ten-point steps.
 * @param reset True to request the pedal controller's configuration reset action.
 * @param frame Destination for the configuration frame.
 */
void pedal_v3_build_configuration(uint8_t brake_force, bool fine_scale, bool reset,
                                  PedalFrame *frame) {
    uint8_t step = fine_scale ? PEDAL_V3_CALIBRATION_BRAKE_STEP : PEDAL_V3_NORMAL_BRAKE_STEP;
    *frame = (PedalFrame){
        .type = PEDAL_V3_CONFIGURATION_FRAME,
        .payload = {(uint8_t)(brake_force / step + 1), reset ? UINT8_MAX : 0},
    };
}

/**
 * @brief Builds the zero-payload V3 calibration keepalive frame.
 * @param frame Destination for the keepalive frame.
 */
void pedal_v3_build_keepalive(PedalFrame *frame) {
    *frame = (PedalFrame){
        .type = PEDAL_V3_KEEPALIVE_FRAME,
    };
}
