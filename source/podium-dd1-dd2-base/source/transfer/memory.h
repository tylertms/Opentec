#ifndef OPENTEC_BASE_MEMORY_TRANSFER_H
#define OPENTEC_BASE_MEMORY_TRANSFER_H

#include <stdint.h>

enum {
    MEMORY_TRANSFER_COMMAND = 4,
    MEMORY_TRANSFER_HEADER_SIZE = 3,
    MEMORY_TRANSFER_READ_REQUEST_SIZE = 5,
    MEMORY_TRANSFER_MAX_READ_SIZE = 512,
    MEMORY_TRANSFER_MAX_WRITE_SIZE = 512,
    MEMORY_TRANSFER_MAX_REQUEST_SIZE = MEMORY_TRANSFER_HEADER_SIZE + MEMORY_TRANSFER_MAX_WRITE_SIZE,
};

typedef enum {
    MEMORY_TRANSFER_ACCEPTED,
    MEMORY_TRANSFER_REJECTED,
    MEMORY_TRANSFER_INVALID_RESPONSE,
} MemoryTransferResult;

uint8_t memory_transfer_encode_read(uint8_t owner, uint8_t offset, uint16_t length,
                                    uint8_t output[MEMORY_TRANSFER_READ_REQUEST_SIZE]);
uint16_t memory_transfer_encode_write(uint8_t owner, uint8_t offset, const uint8_t *data,
                                      uint16_t length,
                                      uint8_t output[MEMORY_TRANSFER_MAX_REQUEST_SIZE]);
MemoryTransferResult memory_transfer_decode_read(const uint8_t *response, uint16_t response_length,
                                                 uint8_t *output, uint16_t output_length);
MemoryTransferResult memory_transfer_decode_write(const uint8_t *response,
                                                  uint16_t response_length);

#endif
