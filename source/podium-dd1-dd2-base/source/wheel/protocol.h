#ifndef OPENTEC_BASE_WHEEL_PROTOCOL_H
#define OPENTEC_BASE_WHEEL_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "input/button_filter.h"

enum {
    WHEEL_PROTOCOL_PACKET_SIZE = 57,
    WHEEL_PROTOCOL_CONTENT_SIZE = 32,
    WHEEL_PROTOCOL_DATA_SIZE = 31,
    WHEEL_PROTOCOL_CHECKSUM_OFFSET = 32,
    WHEEL_PROTOCOL_FLAGS_OFFSET = 56,
    WHEEL_PROTOCOL_REQUEST_READY = 0x02,
    WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED = 0x01,
    WHEEL_PROTOCOL_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY = 0xc1,
    WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY = 0x81,
    WHEEL_MODE_BOOT = 0x00,
    WHEEL_MODE_SCAN_PRIMARY = 0x07,
    WHEEL_MODE_SCAN_SECONDARY = 0x08,
    WHEEL_MODE_MAXIMUM = 0x1e,
};

typedef enum {
    WHEEL_PROTOCOL_WAITING,
    WHEEL_PROTOCOL_SELECTING,
    WHEEL_PROTOCOL_ACTIVE,
    WHEEL_PROTOCOL_AUTHENTICATING,
    WHEEL_PROTOCOL_SCANNING_PRIMARY,
    WHEEL_PROTOCOL_SCANNING_SECONDARY,
} WheelProtocolPhase;

typedef struct {
    uint8_t buttons[3];
    uint8_t axis_outputs[2];
    int8_t motion;
    uint8_t controls[8];
    uint8_t axis_values[2];
    uint8_t mode_buttons;
    uint16_t capability_flags;
    uint8_t axis_limit;
    bool axis_report_enabled;
    bool ready;
} WheelProtocolInput;

typedef struct {
    uint8_t display_segments[3];
    uint8_t display_value;
    uint8_t display_status;
    uint8_t legacy_axes[2];
} WheelProtocolOutput;

typedef struct {
    uint8_t response[WHEEL_PROTOCOL_PACKET_SIZE];
    WheelProtocolInput input;
    WheelProtocolOutput output;
    WheelButtonFilter button_filter;
    WheelProtocolPhase phase;
    uint8_t mode;
} WheelProtocol;

void wheel_protocol_init(WheelProtocol *protocol);
void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]);
const uint8_t *wheel_protocol_response(const WheelProtocol *protocol);
const WheelProtocolInput *wheel_protocol_input(const WheelProtocol *protocol);
void wheel_protocol_set_output(WheelProtocol *protocol, const WheelProtocolOutput *output);
uint8_t wheel_protocol_message_checksum(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);
bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);
bool wheel_protocol_mode_requires_authentication(uint8_t mode);

#endif
