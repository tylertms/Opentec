#ifndef OPENTEC_BASE_WHEEL_MEMORY_TRANSFER_H
#define OPENTEC_BASE_WHEEL_MEMORY_TRANSFER_H

#include <stdint.h>

enum {
    WHEEL_MEMORY_TRANSFER_COMMAND = 4,
    WHEEL_MEMORY_TRANSFER_HEADER_SIZE = 3,
    WHEEL_MEMORY_TRANSFER_READ_REQUEST_SIZE = 5,
    WHEEL_MEMORY_TRANSFER_MAX_READ_SIZE = 512,
    WHEEL_MEMORY_TRANSFER_MAX_WRITE_SIZE = 512,
    WHEEL_MEMORY_TRANSFER_MAX_REQUEST_SIZE =
        WHEEL_MEMORY_TRANSFER_HEADER_SIZE + WHEEL_MEMORY_TRANSFER_MAX_WRITE_SIZE,
};

typedef enum {
    WHEEL_MEMORY_TRANSFER_ACCEPTED,
    WHEEL_MEMORY_TRANSFER_REJECTED,
    WHEEL_MEMORY_TRANSFER_INVALID_RESPONSE,
} WheelMemoryTransferResult;

uint8_t wheel_memory_transfer_encode_read(uint8_t owner, uint8_t offset, uint16_t length,
                                          uint8_t output[WHEEL_MEMORY_TRANSFER_READ_REQUEST_SIZE]);
uint16_t wheel_memory_transfer_encode_write(uint8_t owner, uint8_t offset, const uint8_t *data,
                                            uint16_t length,
                                            uint8_t output[WHEEL_MEMORY_TRANSFER_MAX_REQUEST_SIZE]);
WheelMemoryTransferResult wheel_memory_transfer_decode_read(const uint8_t *response,
                                                            uint16_t response_length,
                                                            uint8_t *output,
                                                            uint16_t output_length);
WheelMemoryTransferResult wheel_memory_transfer_decode_write(const uint8_t *response,
                                                             uint16_t response_length);

#endif
