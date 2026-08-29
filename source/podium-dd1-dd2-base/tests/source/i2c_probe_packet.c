#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe_packet.h"

static uint32_t read_u32_le(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) | ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

static void write_u32_le(uint8_t *destination, uint32_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static void test_calculates_accessory_crc(void) {
    static const uint8_t input[] = "123456789";
    assert(i2c_probe_packet_crc32(input, sizeof(input) - 1) == UINT32_C(0x989ce17d));
    assert(i2c_probe_packet_crc32(0, 10) == 0);
}

static void test_decodes_packet_format(void) {
    const uint8_t packet[] = {0xf3, 0x00, 56, 0x80 | 56};
    I2cProbePacketFormat format;
    assert(i2c_probe_packet_format_decode(packet, sizeof(packet), &format));
    assert(format.receive_chunk_size == 56);
    assert(format.transmit_chunk_size == 56);
    assert(format.checksum_enabled);

    const uint8_t unchecked[] = {0xf3, 0x00, 60, 60};
    assert(i2c_probe_packet_format_decode(unchecked, sizeof(unchecked), &format));
    assert(!format.checksum_enabled);
}

static void test_rejects_invalid_packet_format(void) {
    I2cProbePacketFormat format;
    const uint8_t wrong[] = {0xf4, 0, 56, 56};
    const uint8_t zero[] = {0xf3, 0, 0, 56};
    const uint8_t oversized[] = {0xf3, 0, 57, 0x80 | 56};
    assert(!i2c_probe_packet_format_decode(wrong, sizeof(wrong), &format));
    assert(!i2c_probe_packet_format_decode(zero, sizeof(zero), &format));
    assert(!i2c_probe_packet_format_decode(oversized, sizeof(oversized), &format));
}

static void test_decodes_checked_receive_chunk(void) {
    I2cProbePacketFormat format = {
        .receive_chunk_size = 56,
        .transmit_chunk_size = 56,
        .checksum_enabled = true,
    };
    uint8_t packet[I2C_PROBE_PACKET_SIZE] = {0xf0, 7, 4};
    for (uint8_t index = 0; index < format.receive_chunk_size; ++index) {
        packet[I2C_PROBE_PACKET_PAYLOAD_OFFSET + index] = index;
    }
    write_u32_le(packet + I2C_PROBE_PACKET_CHECKSUM_OFFSET,
                 i2c_probe_packet_crc32(packet, I2C_PROBE_PACKET_CHECKSUM_OFFSET));

    I2cProbePacketChunk chunk;
    assert(i2c_probe_packet_chunk_decode(&format, packet, sizeof(packet), &chunk) ==
           I2C_PROBE_PACKET_CHUNK_VALID);
    assert(chunk.sequence == 7);
    assert(chunk.index == 4);
    assert(chunk.payload == packet + I2C_PROBE_PACKET_PAYLOAD_OFFSET);
    assert(chunk.payload_length == 56);

    packet[10] ^= 1;
    assert(i2c_probe_packet_chunk_decode(&format, packet, sizeof(packet), &chunk) ==
           I2C_PROBE_PACKET_CHUNK_CHECKSUM_ERROR);
}

static void test_encodes_status_packet(void) {
    I2cProbePacketFormat format = {.checksum_enabled = true};
    uint8_t packet[I2C_PROBE_PACKET_STATUS_SIZE];
    assert(i2c_probe_packet_status_encode(&format, 9, 0x10, packet, sizeof(packet)));
    assert(packet[0] == 0xf2);
    assert(packet[1] == 9);
    assert(packet[2] == 0x10);
    assert(read_u32_le(packet + I2C_PROBE_PACKET_STATUS_CHECKSUM_OFFSET) ==
           i2c_probe_packet_crc32(packet, I2C_PROBE_PACKET_STATUS_CHECKSUM_OFFSET));
}

static void test_encodes_checked_response_chunk(void) {
    I2cProbePacketFormat format = {
        .transmit_chunk_size = 56,
        .checksum_enabled = true,
    };
    uint8_t payload[32];
    memset(payload, 0x5a, sizeof(payload));
    uint8_t packet[I2C_PROBE_PACKET_SIZE];
    assert(i2c_probe_packet_chunk_encode(&format, 3, 18, payload, sizeof(payload), packet,
                                         sizeof(packet)));
    assert(packet[0] == 0xf1);
    assert(packet[1] == 3);
    assert(packet[2] == 18);
    assert(memcmp(packet + I2C_PROBE_PACKET_PAYLOAD_OFFSET, payload, sizeof(payload)) == 0);
    assert(read_u32_le(packet + I2C_PROBE_PACKET_CHECKSUM_OFFSET) ==
           i2c_probe_packet_crc32(packet, I2C_PROBE_PACKET_CHECKSUM_OFFSET));
}

static void test_rejects_invalid_chunks(void) {
    I2cProbePacketFormat format = {
        .receive_chunk_size = 56,
        .transmit_chunk_size = 56,
        .checksum_enabled = true,
    };
    uint8_t packet[I2C_PROBE_PACKET_SIZE] = {0};
    I2cProbePacketChunk chunk;
    assert(i2c_probe_packet_chunk_decode(&format, packet, sizeof(packet), &chunk) ==
           I2C_PROBE_PACKET_CHUNK_WRONG_COMMAND);
    assert(i2c_probe_packet_chunk_decode(&format, packet, sizeof(packet) - 1, &chunk) ==
           I2C_PROBE_PACKET_CHUNK_INVALID_LENGTH);
    format.receive_chunk_size = 57;
    assert(i2c_probe_packet_chunk_decode(&format, packet, sizeof(packet), &chunk) ==
           I2C_PROBE_PACKET_CHUNK_INVALID_LENGTH);
    assert(!i2c_probe_packet_chunk_encode(&format, 0, 0, packet, 57, packet, sizeof(packet)));
}

int main(void) {
    test_calculates_accessory_crc();
    test_decodes_packet_format();
    test_rejects_invalid_packet_format();
    test_decodes_checked_receive_chunk();
    test_encodes_status_packet();
    test_encodes_checked_response_chunk();
    test_rejects_invalid_chunks();
    return 0;
}
