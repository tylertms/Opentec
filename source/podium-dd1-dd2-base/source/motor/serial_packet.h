#ifndef OPENTEC_BASE_MOTOR_SERIAL_PACKET_H
#define OPENTEC_BASE_MOTOR_SERIAL_PACKET_H

#include <stdbool.h>
#include <stdint.h>

enum {
    MOTOR_SERIAL_PACKET_SIZE = 64,
    MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE = 57,
    MOTOR_SERIAL_PACKET_TYPE_MASK = 0x0f,
    MOTOR_SERIAL_PACKET_FIRST_FRAGMENT = 0x10,
    MOTOR_SERIAL_PACKET_CONTINUATION_FRAGMENT = 0x20,
    MOTOR_SERIAL_PACKET_FINAL_FRAGMENT = 0x40,
};

typedef enum {
    MOTOR_SERIAL_PACKET_VALID,
    MOTOR_SERIAL_PACKET_INVALID_BOUNDARY,
    MOTOR_SERIAL_PACKET_INVALID_LENGTH,
    MOTOR_SERIAL_PACKET_INVALID_CHECKSUM,
} MotorSerialPacketResult;

typedef struct {
    uint8_t type_flags;
    uint8_t sequence;
    uint8_t payload[MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE];
    uint8_t payload_length;
} MotorSerialPacket;

bool motor_serial_packet_encode(uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                                uint8_t payload_length, uint8_t output[MOTOR_SERIAL_PACKET_SIZE]);
MotorSerialPacketResult motor_serial_packet_decode(const uint8_t input[MOTOR_SERIAL_PACKET_SIZE],
                                                   MotorSerialPacket *packet);

#endif
