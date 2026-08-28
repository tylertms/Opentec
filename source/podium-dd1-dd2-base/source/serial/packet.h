#ifndef OPENTEC_BASE_SERIAL_PACKET_H
#define OPENTEC_BASE_SERIAL_PACKET_H

#include <stdbool.h>
#include <stdint.h>

enum {
    SERIAL_PACKET_SIZE = 64,
    SERIAL_PACKET_MAX_PAYLOAD_SIZE = 57,
    SERIAL_PACKET_START = 0x7b,
    SERIAL_PACKET_END = 0x7d,
    SERIAL_PACKET_TYPE_MASK = 0x0f,
    SERIAL_PACKET_FIRST_FRAGMENT = 0x10,
    SERIAL_PACKET_CONTINUATION_FRAGMENT = 0x20,
    SERIAL_PACKET_FINAL_FRAGMENT = 0x40,
};

typedef enum {
    SERIAL_PACKET_VALID,
    SERIAL_PACKET_INVALID_BOUNDARY,
    SERIAL_PACKET_INVALID_LENGTH,
    SERIAL_PACKET_INVALID_CHECKSUM,
} SerialPacketResult;

typedef struct {
    uint8_t type_flags;
    uint8_t sequence;
    uint8_t payload[SERIAL_PACKET_MAX_PAYLOAD_SIZE];
    uint8_t payload_length;
} SerialPacket;

bool serial_packet_encode(uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                          uint8_t payload_length, uint8_t output[SERIAL_PACKET_SIZE]);
bool serial_packet_encode_byte(uint8_t type_flags, uint8_t sequence, uint8_t payload,
                               uint8_t output[SERIAL_PACKET_SIZE]);
SerialPacketResult serial_packet_decode(const uint8_t input[SERIAL_PACKET_SIZE],
                                        SerialPacket *packet);

#endif
