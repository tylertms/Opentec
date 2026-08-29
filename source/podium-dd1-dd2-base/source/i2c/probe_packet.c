#include "i2c/probe_packet.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    I2C_PROBE_PACKET_FORMAT_COMMAND = 0xf3,
    I2C_PROBE_PACKET_RECEIVE_COMMAND = 0xf0,
    I2C_PROBE_PACKET_TRANSMIT_COMMAND = 0xf1,
    I2C_PROBE_PACKET_STATUS_COMMAND = 0xf2,
    I2C_PROBE_PACKET_CRC_INPUT_SIZE = 60,
    I2C_PROBE_PACKET_STATUS_CRC_INPUT_SIZE = 12,
};

/**
 * @brief Writes a 32-bit packet field in little-endian order.
 *
 * Stores the least-significant byte first in the four-byte destination field.
 *
 * @param[out] destination Four-byte destination field.
 * @param[in] value Value to encode.
 */
static void write_u32_le(uint8_t *destination, uint32_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Reads a 32-bit packet field in little-endian order.
 *
 * Reconstructs a 32-bit value from four bytes with the least-significant byte first.
 *
 * @param[in] source Four-byte encoded field.
 * @return Decoded 32-bit value.
 */
static uint32_t read_u32_le(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) | ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

/**
 * @brief Calculates the accessory packet cyclic redundancy check.
 *
 * Starts with all bits set, folds each byte least-significant bit first with polynomial
 * 0x77073096, and inverts the final 32-bit value.
 *
 * @param[in] data Bytes to include, or null for an empty input.
 * @param[in] length Number of bytes to include.
 * @return Inverted 32-bit cyclic redundancy check.
 */
uint32_t i2c_probe_packet_crc32(const uint8_t *data, uint8_t length) {
    uint32_t crc = UINT32_MAX;
    if (data != 0) {
        for (uint8_t index = 0; index < length; ++index) {
            crc ^= data[index];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0x77073096));
            }
        }
    }
    return ~crc;
}

/**
 * @brief Decodes an accessory transfer format packet.
 *
 * Accepts command 0xf3, extracts both seven-bit chunk sizes, and uses the high bit of the
 * transmit-size byte to select packet checksums.
 *
 * @param[in] packet Encoded format packet.
 * @param[in] length Available packet bytes.
 * @param[out] format Decoded receive size, transmit size, and checksum mode.
 * @return True for a complete format packet with nonzero usable chunk sizes; otherwise false.
 */
bool i2c_probe_packet_format_decode(const uint8_t *packet, uint8_t length,
                                    I2cProbePacketFormat *format) {
    if (packet == 0 || format == 0 || length < 4 || packet[0] != I2C_PROBE_PACKET_FORMAT_COMMAND) {
        return false;
    }

    I2cProbePacketFormat decoded = {
        .receive_chunk_size = packet[2] & 0x7fu,
        .transmit_chunk_size = packet[3] & 0x7fu,
        .checksum_enabled = (packet[3] & 0x80u) != 0,
    };
    uint8_t capacity = decoded.checksum_enabled
                           ? I2C_PROBE_PACKET_CHECKSUM_OFFSET - I2C_PROBE_PACKET_PAYLOAD_OFFSET
                           : I2C_PROBE_PACKET_SIZE - I2C_PROBE_PACKET_PAYLOAD_OFFSET;
    if (decoded.receive_chunk_size == 0 || decoded.transmit_chunk_size == 0 ||
        decoded.receive_chunk_size > capacity || decoded.transmit_chunk_size > capacity) {
        return false;
    }

    *format = decoded;
    return true;
}

/**
 * @brief Decodes one host-to-accessory transfer chunk.
 *
 * Accepts command 0xf0, exposes its sequence, fragment index, and configured payload span, and
 * validates the little-endian checksum over the first 60 bytes when enabled.
 *
 * @param[in] format Active packet chunk sizes and checksum mode.
 * @param[in] packet Encoded 64-byte transfer chunk.
 * @param[in] length Available packet bytes.
 * @param[out] chunk Decoded sequence, index, and payload view.
 * @return Valid, wrong-command, invalid-length, or checksum-error result.
 */
I2cProbePacketChunkResult i2c_probe_packet_chunk_decode(const I2cProbePacketFormat *format,
                                                        const uint8_t *packet, uint8_t length,
                                                        I2cProbePacketChunk *chunk) {
    uint8_t capacity = format != 0 && format->checksum_enabled
                           ? I2C_PROBE_PACKET_CHECKSUM_OFFSET - I2C_PROBE_PACKET_PAYLOAD_OFFSET
                           : I2C_PROBE_PACKET_SIZE - I2C_PROBE_PACKET_PAYLOAD_OFFSET;
    if (format == 0 || packet == 0 || chunk == 0 || length != I2C_PROBE_PACKET_SIZE ||
        format->receive_chunk_size == 0 || format->receive_chunk_size > capacity) {
        return I2C_PROBE_PACKET_CHUNK_INVALID_LENGTH;
    }
    if (packet[0] != I2C_PROBE_PACKET_RECEIVE_COMMAND) {
        return I2C_PROBE_PACKET_CHUNK_WRONG_COMMAND;
    }
    if (format->checksum_enabled &&
        read_u32_le(packet + I2C_PROBE_PACKET_CHECKSUM_OFFSET) !=
            i2c_probe_packet_crc32(packet, I2C_PROBE_PACKET_CRC_INPUT_SIZE)) {
        return I2C_PROBE_PACKET_CHUNK_CHECKSUM_ERROR;
    }

    *chunk = (I2cProbePacketChunk){
        .sequence = packet[1],
        .index = packet[2],
        .payload = packet + I2C_PROBE_PACKET_PAYLOAD_OFFSET,
        .payload_length = format->receive_chunk_size,
    };
    return I2C_PROBE_PACKET_CHUNK_VALID;
}

/**
 * @brief Encodes an accessory transfer status packet.
 *
 * Clears 16 bytes, writes command 0xf2 with the sequence and status, and optionally appends the
 * little-endian checksum of the first 12 bytes.
 *
 * @param[in] format Active packet checksum mode.
 * @param[in] sequence Transfer sequence value.
 * @param[in] status Transfer status value.
 * @param[out] packet Destination for the 16-byte status packet.
 * @param[in] length Available destination bytes.
 * @return True when the status packet is encoded; otherwise false.
 */
bool i2c_probe_packet_status_encode(const I2cProbePacketFormat *format, uint8_t sequence,
                                    uint8_t status, uint8_t *packet, uint8_t length) {
    if (format == 0 || packet == 0 || length < I2C_PROBE_PACKET_STATUS_SIZE) {
        return false;
    }

    memset(packet, 0, I2C_PROBE_PACKET_STATUS_SIZE);
    packet[0] = I2C_PROBE_PACKET_STATUS_COMMAND;
    packet[1] = sequence;
    packet[2] = status;
    if (format->checksum_enabled) {
        write_u32_le(packet + I2C_PROBE_PACKET_STATUS_CHECKSUM_OFFSET,
                     i2c_probe_packet_crc32(packet, I2C_PROBE_PACKET_STATUS_CRC_INPUT_SIZE));
    }
    return true;
}

/**
 * @brief Encodes one accessory-to-host transfer chunk.
 *
 * Clears 64 bytes, writes command 0xf1 with the sequence and fragment index, copies the selected
 * response bytes at offset four, and optionally appends the little-endian checksum of the first 60
 * bytes.
 *
 * @param[in] format Active packet transmit size and checksum mode.
 * @param[in] sequence Transfer sequence value.
 * @param[in] index Response fragment index.
 * @param[in] payload Response bytes for this fragment.
 * @param[in] payload_length Number of response bytes to copy.
 * @param[out] packet Destination for the 64-byte transfer packet.
 * @param[in] length Available destination bytes.
 * @return True when the transfer packet is encoded; otherwise false.
 */
bool i2c_probe_packet_chunk_encode(const I2cProbePacketFormat *format, uint8_t sequence,
                                   uint8_t index, const uint8_t *payload, uint8_t payload_length,
                                   uint8_t *packet, uint8_t length) {
    uint8_t capacity = format != 0 && format->checksum_enabled
                           ? I2C_PROBE_PACKET_CHECKSUM_OFFSET - I2C_PROBE_PACKET_PAYLOAD_OFFSET
                           : I2C_PROBE_PACKET_SIZE - I2C_PROBE_PACKET_PAYLOAD_OFFSET;
    if (format == 0 || packet == 0 || length < I2C_PROBE_PACKET_SIZE || payload_length > capacity ||
        payload_length > format->transmit_chunk_size || (payload_length != 0 && payload == 0)) {
        return false;
    }

    memset(packet, 0, I2C_PROBE_PACKET_SIZE);
    packet[0] = I2C_PROBE_PACKET_TRANSMIT_COMMAND;
    packet[1] = sequence;
    packet[2] = index;
    if (payload_length != 0) {
        memcpy(packet + I2C_PROBE_PACKET_PAYLOAD_OFFSET, payload, payload_length);
    }
    if (format->checksum_enabled) {
        write_u32_le(packet + I2C_PROBE_PACKET_CHECKSUM_OFFSET,
                     i2c_probe_packet_crc32(packet, I2C_PROBE_PACKET_CRC_INPUT_SIZE));
    }
    return true;
}
