#ifndef OPENTEC_BASE_I2C_PROBE_PACKET_H
#define OPENTEC_BASE_I2C_PROBE_PACKET_H

#include <stdbool.h>
#include <stdint.h>

enum {
    I2C_PROBE_PACKET_SIZE = 64,
    I2C_PROBE_PACKET_PAYLOAD_OFFSET = 4,
    I2C_PROBE_PACKET_CHECKSUM_OFFSET = 60,
    I2C_PROBE_PACKET_STATUS_SIZE = 16,
    I2C_PROBE_PACKET_STATUS_CHECKSUM_OFFSET = 12,
};

typedef struct {
    uint8_t receive_chunk_size;
    uint8_t transmit_chunk_size;
    bool checksum_enabled;
} I2cProbePacketFormat;

typedef struct {
    uint8_t sequence;
    uint8_t index;
    const uint8_t *payload;
    uint8_t payload_length;
} I2cProbePacketChunk;

typedef enum {
    I2C_PROBE_PACKET_CHUNK_VALID,
    I2C_PROBE_PACKET_CHUNK_WRONG_COMMAND,
    I2C_PROBE_PACKET_CHUNK_INVALID_LENGTH,
    I2C_PROBE_PACKET_CHUNK_CHECKSUM_ERROR,
} I2cProbePacketChunkResult;

uint32_t i2c_probe_packet_crc32(const uint8_t *data, uint8_t length);
bool i2c_probe_packet_format_decode(const uint8_t *packet, uint8_t length,
                                    I2cProbePacketFormat *format);
I2cProbePacketChunkResult i2c_probe_packet_chunk_decode(const I2cProbePacketFormat *format,
                                                        const uint8_t *packet, uint8_t length,
                                                        I2cProbePacketChunk *chunk);
bool i2c_probe_packet_status_encode(const I2cProbePacketFormat *format, uint8_t sequence,
                                    uint8_t status, uint8_t *packet, uint8_t length);
bool i2c_probe_packet_chunk_encode(const I2cProbePacketFormat *format, uint8_t sequence,
                                   uint8_t index, const uint8_t *payload, uint8_t payload_length,
                                   uint8_t *packet, uint8_t length);

#endif
