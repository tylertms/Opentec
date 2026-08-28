#include "wheel/protocol.h"

#include <stdbool.h>
#include <stdint.h>

#include "wheel/authentication.h"
#include "wheel/packet_mode_one.h"

/**
 * Calculates the attached-wheel message CRC-8 with an initial value of 0xFF.
 *
 * @param data First byte covered by the checksum.
 * @param length Number of bytes to process.
 * @return Reflected CRC-8 value using polynomial 0x8C.
 */
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

static void clear(uint8_t *data, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        data[index] = 0;
    }
}

static void build_selection_response(WheelProtocol *protocol) {
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    protocol->response[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

static void build_active_response(WheelProtocol *protocol) {
    if (!wheel_packet_mode_one_applies(protocol->mode)) {
        return;
    }
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    wheel_packet_mode_one_encode(&protocol->mode_one_output, protocol->response);
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

/**
 * Stores the first 30 bytes of an active attached-wheel request and detects changes.
 *
 * @param protocol Protocol state that owns the request snapshot and change latch.
 * @param request Complete 57-byte attached-wheel request.
 */
static void capture_request(WheelProtocol *protocol,
                            const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    bool changed = false;
    for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
        changed |= protocol->request[index] != request[index];
        protocol->request[index] = request[index];
    }
    protocol->request_ready = true;
    protocol->request_changed |= changed;
}

static void select_mode(WheelProtocol *protocol,
                        const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
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
            protocol->phase = WHEEL_PROTOCOL_UNSUPPORTED;
            break;
        }
        protocol->mode = request[1];
        if (wheel_authentication_required(protocol->mode_one_output.operating_mode)) {
            wheel_authentication_init(&protocol->authentication,
                                      protocol->mode_one_output.operating_mode);
            protocol->phase = WHEEL_PROTOCOL_AUTHENTICATING;
        } else {
            protocol->phase = WHEEL_PROTOCOL_ACTIVE;
        }
        break;
    default:
        return;
    }
    build_selection_response(protocol);
}

/**
 * Checks the command byte accepted by the active attached-wheel exchange.
 *
 * @param protocol Protocol state with the active operating mode.
 * @param command Received command byte.
 * @return True for A6 or A7 in authenticated modes, or A5 in other modes.
 */
static bool active_command_valid(const WheelProtocol *protocol, uint8_t command) {
    if (wheel_authentication_required(protocol->mode_one_output.operating_mode)) {
        return command == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE ||
               command == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE_REPLY;
    }
    return command == WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
}

void wheel_protocol_init(WheelProtocol *protocol) {
    const WheelPacketModeOneOutput empty_output = {0};
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    clear(protocol->request, WHEEL_PROTOCOL_SNAPSHOT_SIZE);
    protocol->mode_one_output = empty_output;
    wheel_authentication_init(&protocol->authentication, WHEEL_MODE_UNKNOWN);
    protocol->phase = WHEEL_PROTOCOL_WAITING;
    protocol->mode = WHEEL_MODE_UNKNOWN;
    protocol->request_ready = false;
    protocol->request_changed = false;
}

void wheel_protocol_set_mode_one_output(WheelProtocol *protocol,
                                        const WheelPacketModeOneOutput *output) {
    protocol->mode_one_output = *output;
}

void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    bool ready = (request[WHEEL_PROTOCOL_FLAGS_OFFSET] & WHEEL_PROTOCOL_REQUEST_READY) != 0;

    switch (protocol->phase) {
    case WHEEL_PROTOCOL_WAITING:
        if (ready) {
            protocol->phase = WHEEL_PROTOCOL_SYNCHRONIZING;
        }
        return;
    case WHEEL_PROTOCOL_SYNCHRONIZING:
        if (!ready) {
            protocol->phase = WHEEL_PROTOCOL_WAITING;
            return;
        }
        protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] |= WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED;
        protocol->phase = WHEEL_PROTOCOL_ACKNOWLEDGING;
        return;
    case WHEEL_PROTOCOL_ACKNOWLEDGING:
        protocol->phase = ready ? WHEEL_PROTOCOL_SELECTING : WHEEL_PROTOCOL_WAITING;
        return;
    case WHEEL_PROTOCOL_SELECTING:
        if (!ready) {
            protocol->phase = WHEEL_PROTOCOL_WAITING;
            return;
        }
        select_mode(protocol, request);
        return;
    case WHEEL_PROTOCOL_AUTHENTICATING:
        if (ready && wheel_authentication_accept(&protocol->authentication, request,
                                                 wheel_protocol_message_valid(request),
                                                 protocol->response)) {
            protocol->phase = WHEEL_PROTOCOL_ACTIVE;
        }
        if (ready) {
            protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
                crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
        }
        return;
    case WHEEL_PROTOCOL_ACTIVE:
        if (!ready) {
            return;
        }
        if (wheel_protocol_message_valid(request)) {
            if (active_command_valid(protocol, request[0])) {
                capture_request(protocol, request);
            } else if (wheel_authentication_required(protocol->mode_one_output.operating_mode)) {
                wheel_authentication_init(&protocol->authentication,
                                          protocol->mode_one_output.operating_mode);
                protocol->phase = WHEEL_PROTOCOL_AUTHENTICATING;
            }
        }
        build_active_response(protocol);
        return;
    case WHEEL_PROTOCOL_UNSUPPORTED:
    case WHEEL_PROTOCOL_SCANNING_PRIMARY:
    case WHEEL_PROTOCOL_SCANNING_SECONDARY:
        return;
    }
}

const uint8_t *wheel_protocol_response(const WheelProtocol *protocol) { return protocol->response; }

const uint8_t *wheel_protocol_request(const WheelProtocol *protocol) {
    return protocol->request_ready ? protocol->request : 0;
}

bool wheel_protocol_request_changed(WheelProtocol *protocol) {
    bool changed = protocol->request_changed;
    protocol->request_changed = false;
    return changed;
}

uint8_t wheel_protocol_message_checksum(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return crc8(packet, WHEEL_PROTOCOL_CONTENT_SIZE);
}

bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return packet[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == wheel_protocol_message_checksum(packet);
}
