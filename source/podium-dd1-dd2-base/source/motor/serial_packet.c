#include "motor/serial_packet.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    MOTOR_SERIAL_PACKET_START = 0x7b,
    MOTOR_SERIAL_PACKET_END = 0x7d,
    MOTOR_SERIAL_PACKET_TYPE_OFFSET = 1,
    MOTOR_SERIAL_PACKET_SEQUENCE_OFFSET = 2,
    MOTOR_SERIAL_PACKET_LENGTH_OFFSET = 3,
    MOTOR_SERIAL_PACKET_PAYLOAD_OFFSET = 4,
    MOTOR_SERIAL_PACKET_CHECKSUM_LOW_OFFSET = 61,
    MOTOR_SERIAL_PACKET_CHECKSUM_HIGH_OFFSET = 62,
    MOTOR_SERIAL_PACKET_CHECKSUM_SIZE = 60,
};

/**
 * @brief Updates a motor serial packet checksum.
 *
 * Applies the nibble-reduced 16-bit recurrence to one byte of the fixed packet checksum region.
 *
 * @param[in] checksum Current checksum value.
 * @param[in] data Next packet byte.
 * @return Updated checksum value.
 */
static uint16_t update_checksum(uint16_t checksum, uint8_t data) {
    uint16_t mixed = (uint8_t)data ^ checksum;
    uint16_t low = (uint8_t)mixed;
    uint16_t nibble = mixed & 0x0f;
    mixed = low ^ (nibble << 4);
    uint16_t high = mixed >> 4;
    uint16_t result = (checksum >> 8) ^ high;
    return result ^ ((((mixed + mixed) ^ high) << 4) ^ nibble) << 3;
}

/**
 * @brief Calculates a motor serial packet checksum.
 *
 * Starts at zero and folds the fixed sixty-byte region from the type byte through the final
 * payload-storage byte.
 *
 * @param[in] data Packet bytes beginning at the type field.
 * @return Calculated checksum value.
 */
static uint16_t checksum(const uint8_t *data) {
    uint16_t value = 0;
    for (uint8_t index = 0; index < MOTOR_SERIAL_PACKET_CHECKSUM_SIZE; index++) {
        value = update_checksum(value, data[index]);
    }
    return value;
}

/**
 * @brief Encodes one fixed motor serial transport packet.
 *
 * Clears all sixty-four bytes, writes the boundaries, type and fragment flags, sequence, length,
 * and up to fifty-seven payload bytes, then stores the checksum low byte first.
 *
 * @param[in] type_flags Low-nibble packet type and optional fragment flags.
 * @param[in] sequence Transport sequence byte.
 * @param[in] payload Packet payload, or null when the payload is empty.
 * @param[in] payload_length Payload byte count from zero through fifty-seven.
 * @param[out] output Encoded sixty-four-byte packet.
 * @return True when the payload and destination are usable.
 */
bool motor_serial_packet_encode(uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                                uint8_t payload_length, uint8_t output[MOTOR_SERIAL_PACKET_SIZE]) {
    if (output == 0 || payload_length > MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE ||
        (payload == 0 && payload_length != 0)) {
        return false;
    }
    memset(output, 0, MOTOR_SERIAL_PACKET_SIZE);
    output[0] = MOTOR_SERIAL_PACKET_START;
    output[MOTOR_SERIAL_PACKET_TYPE_OFFSET] = type_flags;
    output[MOTOR_SERIAL_PACKET_SEQUENCE_OFFSET] = sequence;
    output[MOTOR_SERIAL_PACKET_LENGTH_OFFSET] = payload_length;
    if (payload_length != 0) {
        memcpy(output + MOTOR_SERIAL_PACKET_PAYLOAD_OFFSET, payload, payload_length);
    }
    uint16_t value = checksum(output + MOTOR_SERIAL_PACKET_TYPE_OFFSET);
    output[MOTOR_SERIAL_PACKET_CHECKSUM_LOW_OFFSET] = (uint8_t)value;
    output[MOTOR_SERIAL_PACKET_CHECKSUM_HIGH_OFFSET] = (uint8_t)(value >> 8);
    output[MOTOR_SERIAL_PACKET_SIZE - 1] = MOTOR_SERIAL_PACKET_END;
    return true;
}

/**
 * @brief Validates and decodes one fixed motor serial transport packet.
 *
 * Checks both boundaries, the bounded payload length, and the checksum across the fixed sixty-byte
 * region before publishing the type flags, sequence, and logical payload.
 *
 * @param[in] input Received sixty-four-byte packet.
 * @param[out] packet Decoded packet fields and payload.
 * @return Boundary, length, checksum, or valid-packet result.
 */
MotorSerialPacketResult motor_serial_packet_decode(const uint8_t input[MOTOR_SERIAL_PACKET_SIZE],
                                                   MotorSerialPacket *packet) {
    if (input == 0 || packet == 0 || input[0] != MOTOR_SERIAL_PACKET_START ||
        input[MOTOR_SERIAL_PACKET_SIZE - 1] != MOTOR_SERIAL_PACKET_END) {
        return MOTOR_SERIAL_PACKET_INVALID_BOUNDARY;
    }
    uint8_t payload_length = input[MOTOR_SERIAL_PACKET_LENGTH_OFFSET];
    if (payload_length > MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE) {
        return MOTOR_SERIAL_PACKET_INVALID_LENGTH;
    }
    uint16_t expected = checksum(input + MOTOR_SERIAL_PACKET_TYPE_OFFSET);
    uint16_t received = input[MOTOR_SERIAL_PACKET_CHECKSUM_LOW_OFFSET] |
                        (uint16_t)input[MOTOR_SERIAL_PACKET_CHECKSUM_HIGH_OFFSET] << 8;
    if (received != expected) {
        return MOTOR_SERIAL_PACKET_INVALID_CHECKSUM;
    }
    packet->type_flags = input[MOTOR_SERIAL_PACKET_TYPE_OFFSET];
    packet->sequence = input[MOTOR_SERIAL_PACKET_SEQUENCE_OFFSET];
    packet->payload_length = payload_length;
    if (payload_length != 0) {
        memcpy(packet->payload, input + MOTOR_SERIAL_PACKET_PAYLOAD_OFFSET, payload_length);
    }
    return MOTOR_SERIAL_PACKET_VALID;
}
