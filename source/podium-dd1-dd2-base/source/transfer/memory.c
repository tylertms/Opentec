#include "transfer/memory.h"

#include <stdint.h>

/**
 * @brief Internal remote memory request and response constants.
 *
 * These values define the memory control header, direction flag, response status, and read-data
 * offset.
 */
enum {
    MEMORY_TRANSFER_CONTROL_COMMAND = 2,   /**< Control-command byte in memory requests. */
    MEMORY_TRANSFER_READ_FLAG = 1,         /**< Direction bit selecting a read request. */
    MEMORY_TRANSFER_RESPONSE_REJECTED = 0, /**< Response status for a rejected request. */
    MEMORY_TRANSFER_RESPONSE_ACCEPTED = 1, /**< Response status for an accepted request. */
    MEMORY_TRANSFER_READ_DATA_OFFSET = 2,  /**< Offset of read data after response status bytes. */
};

/**
 * @brief Encodes a remote memory control header.
 *
 * Packs the control command, shifted owner identifier, direction bit, and remote offset.
 *
 * @param[in] owner Transfer owner identifier.
 * @param[in] direction Zero for writes or one for reads.
 * @param[in] offset Remote memory offset.
 * @param[out] output Three-byte control header.
 */
static void encode_header(uint8_t owner, uint8_t direction, uint8_t offset, uint8_t *output) {
    output[0] = MEMORY_TRANSFER_CONTROL_COMMAND;
    output[1] = (uint8_t)((owner << 1) | direction);
    output[2] = offset;
}

uint8_t memory_transfer_encode_read(uint8_t owner, uint8_t offset, uint16_t length,
                                    uint8_t output[MEMORY_TRANSFER_READ_REQUEST_SIZE]) {
    if (length > MEMORY_TRANSFER_MAX_READ_SIZE) {
        return 0;
    }
    encode_header(owner, MEMORY_TRANSFER_READ_FLAG, offset, output);
    output[3] = (uint8_t)length;
    output[4] = (uint8_t)(length >> 8);
    return MEMORY_TRANSFER_READ_REQUEST_SIZE;
}

uint16_t memory_transfer_encode_write(uint8_t owner, uint8_t offset, const uint8_t *data,
                                      uint16_t length,
                                      uint8_t output[MEMORY_TRANSFER_MAX_REQUEST_SIZE]) {
    if (length > MEMORY_TRANSFER_MAX_WRITE_SIZE || (data == 0 && length != 0)) {
        return 0;
    }
    encode_header(owner, 0, offset, output);
    for (uint16_t index = 0; index < length; index++) {
        output[MEMORY_TRANSFER_HEADER_SIZE + index] = data[index];
    }
    return MEMORY_TRANSFER_HEADER_SIZE + length;
}

/**
 * @brief Decodes a remote memory response status.
 *
 * Maps status zero to rejected, status one to accepted, and every other form to invalid.
 *
 * @param[in] response Received memory response.
 * @param[in] response_length Received byte count.
 * @return Accepted, rejected, or invalid-response status.
 */
static MemoryTransferResult decode_status(const uint8_t *response, uint16_t response_length) {
    if (response == 0 || response_length == 0) {
        return MEMORY_TRANSFER_INVALID_RESPONSE;
    }
    if (response[0] == MEMORY_TRANSFER_RESPONSE_REJECTED) {
        return MEMORY_TRANSFER_REJECTED;
    }
    return response[0] == MEMORY_TRANSFER_RESPONSE_ACCEPTED ? MEMORY_TRANSFER_ACCEPTED
                                                            : MEMORY_TRANSFER_INVALID_RESPONSE;
}

MemoryTransferResult memory_transfer_decode_read(const uint8_t *response, uint16_t response_length,
                                                 uint8_t *output, uint16_t output_length) {
    MemoryTransferResult result = decode_status(response, response_length);
    if (result != MEMORY_TRANSFER_ACCEPTED) {
        return result;
    }
    if ((output == 0 && output_length != 0) ||
        response_length != output_length + MEMORY_TRANSFER_READ_DATA_OFFSET) {
        return MEMORY_TRANSFER_INVALID_RESPONSE;
    }
    for (uint16_t index = 0; index < output_length; index++) {
        output[index] = response[MEMORY_TRANSFER_READ_DATA_OFFSET + index];
    }
    return MEMORY_TRANSFER_ACCEPTED;
}

MemoryTransferResult memory_transfer_decode_write(const uint8_t *response,
                                                  uint16_t response_length) {
    return decode_status(response, response_length);
}
