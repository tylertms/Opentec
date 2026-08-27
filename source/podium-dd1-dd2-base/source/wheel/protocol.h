#ifndef OPENTEC_BASE_WHEEL_PROTOCOL_H
#define OPENTEC_BASE_WHEEL_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

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
    uint8_t response[WHEEL_PROTOCOL_PACKET_SIZE];
    WheelProtocolPhase phase;
    uint8_t mode;
} WheelProtocol;

void wheel_protocol_init(WheelProtocol *protocol);
void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]);
const uint8_t *wheel_protocol_response(const WheelProtocol *protocol);
bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);
bool wheel_protocol_mode_requires_authentication(uint8_t mode);

#endif
