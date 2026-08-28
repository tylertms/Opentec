#ifndef OPENTEC_BASE_WHEEL_PROTOCOL_H
#define OPENTEC_BASE_WHEEL_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/authentication.h"
#include "wheel/axis_override.h"
#include "wheel/packet_mode_one.h"

enum {
    WHEEL_PROTOCOL_PACKET_SIZE = 57,
    WHEEL_PROTOCOL_CONTENT_SIZE = 32,
    WHEEL_PROTOCOL_SNAPSHOT_SIZE = 30,
    WHEEL_PROTOCOL_CHECKSUM_OFFSET = 32,
    WHEEL_PROTOCOL_FLAGS_OFFSET = 56,
    WHEEL_PROTOCOL_REQUEST_READY = 0x02,
    WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED = 0x01,
    WHEEL_PROTOCOL_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PROTOCOL_COMMAND_AUTHENTICATE = 0xa6,
    WHEEL_PROTOCOL_COMMAND_AUTHENTICATE_REPLY = 0xa7,
    WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY = 0xc1,
    WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY = 0x81,
    WHEEL_MODE_UNKNOWN = 0x00,
    WHEEL_MODE_SCAN_PRIMARY = 0x07,
    WHEEL_MODE_SCAN_SECONDARY = 0x08,
    WHEEL_MODE_MAXIMUM = 0x1e,
};

typedef enum {
    WHEEL_PROTOCOL_WAITING,
    WHEEL_PROTOCOL_SYNCHRONIZING,
    WHEEL_PROTOCOL_ACKNOWLEDGING,
    WHEEL_PROTOCOL_SELECTING,
    WHEEL_PROTOCOL_AUTHENTICATING,
    WHEEL_PROTOCOL_ACTIVE,
    WHEEL_PROTOCOL_UNSUPPORTED,
    WHEEL_PROTOCOL_SCANNING_PRIMARY,
    WHEEL_PROTOCOL_SCANNING_SECONDARY,
} WheelProtocolPhase;

typedef struct {
    uint8_t response[WHEEL_PROTOCOL_PACKET_SIZE];
    uint8_t request[WHEEL_PROTOCOL_SNAPSHOT_SIZE];
    WheelAuthentication authentication;
    WheelAxisOverrideProcessor axis_override_processor;
    WheelPacketModeOneButtonFilter mode_one_button_filter;
    WheelPacketModeOneControlAxisFilter mode_one_control_axis_filter;
    WheelPacketModeOneInput mode_one_input;
    WheelPacketModeOneOutput mode_one_output;
    WheelProtocolPhase phase;
    uint8_t mode;
    uint8_t interface_mode;
    uint8_t configured_axis_override_mode;
    uint8_t axis_calibration_value;
    bool request_ready;
    bool request_changed;
} WheelProtocol;

void wheel_protocol_init(WheelProtocol *protocol);
void wheel_protocol_set_mode_one_output(WheelProtocol *protocol,
                                        const WheelPacketModeOneOutput *output);
void wheel_protocol_set_axis_processing(WheelProtocol *protocol, uint8_t interface_mode,
                                        uint8_t override_mode, uint8_t calibration_value);
void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]);
const uint8_t *wheel_protocol_response(const WheelProtocol *protocol);
const uint8_t *wheel_protocol_request(const WheelProtocol *protocol);
const WheelPacketModeOneInput *wheel_protocol_mode_one_input(const WheelProtocol *protocol);
const WheelAxisOverrideProcessor *wheel_protocol_axis_overrides(const WheelProtocol *protocol);
bool wheel_protocol_request_changed(WheelProtocol *protocol);
uint8_t wheel_protocol_message_checksum(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);
bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);

#endif
