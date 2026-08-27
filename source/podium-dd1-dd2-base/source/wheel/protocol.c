#include "wheel/protocol.h"

#include <stdbool.h>
#include <stdint.h>

static uint8_t crc8(const uint8_t *data, uint8_t length) {
    uint8_t crc = UINT8_MAX;
    while (length-- != 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (uint8_t)((crc >> 1) ^ 0x8cu) : (uint8_t)(crc >> 1);
        }
    }
    return crc;
}

static void clear_response(WheelProtocol *protocol) {
    for (uint8_t index = 0; index < WHEEL_PROTOCOL_PACKET_SIZE; index++) {
        protocol->response[index] = 0;
    }
}

static void acknowledge(WheelProtocol *protocol) {
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED;
}

static void respond_to_mode_selection(WheelProtocol *protocol) {
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear_response(protocol);
    protocol->response[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

bool wheel_protocol_mode_requires_authentication(uint8_t mode) {
    switch (mode) {
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0e:
    case 0x0f:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
        return true;
    default:
        return false;
    }
}

void wheel_protocol_init(WheelProtocol *protocol) {
    clear_response(protocol);
    protocol->phase = WHEEL_PROTOCOL_WAITING;
    protocol->mode = WHEEL_MODE_BOOT;
}

void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    if ((request[WHEEL_PROTOCOL_FLAGS_OFFSET] & WHEEL_PROTOCOL_REQUEST_READY) == 0) {
        wheel_protocol_init(protocol);
        return;
    }

    if (protocol->phase == WHEEL_PROTOCOL_WAITING) {
        acknowledge(protocol);
        protocol->phase = WHEEL_PROTOCOL_SELECTING;
        return;
    }
    if (protocol->phase != WHEEL_PROTOCOL_SELECTING) {
        return;
    }

    switch (request[0]) {
    case WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY:
        protocol->mode = WHEEL_MODE_SCAN_PRIMARY;
        protocol->phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
        break;
    case WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY:
        protocol->mode = WHEEL_MODE_SCAN_SECONDARY;
        protocol->phase = WHEEL_PROTOCOL_SCANNING_SECONDARY;
        break;
    case WHEEL_PROTOCOL_COMMAND_SELECT_MODE:
        if (request[1] > WHEEL_MODE_MAXIMUM) {
            return;
        }
        protocol->mode = request[1];
        protocol->phase = wheel_protocol_mode_requires_authentication(protocol->mode)
                              ? WHEEL_PROTOCOL_AUTHENTICATING
                              : WHEEL_PROTOCOL_ACTIVE;
        break;
    default:
        return;
    }
    respond_to_mode_selection(protocol);
}

const uint8_t *wheel_protocol_response(const WheelProtocol *protocol) { return protocol->response; }

bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return packet[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == crc8(packet, WHEEL_PROTOCOL_CONTENT_SIZE);
}
