#ifndef OPENTEC_BASE_MEMORY_TRANSFER_H
#define OPENTEC_BASE_MEMORY_TRANSFER_H

#include <stdint.h>

/**
 * @brief Remote memory-transfer command and buffer limits.
 *
 * These constants describe the group-4 memory request layout and keep every encoded request and
 * accepted read response within the 512-byte serial logical-message buffer. Read and write
 * payload limits reserve space for their response prefix and request header.
 */
enum {
    MEMORY_TRANSFER_COMMAND = 4,         /**< Transfer group used for memory commands. */
    MEMORY_TRANSFER_MAX_WIRE_SIZE = 512, /**< Maximum serial logical-message size in bytes. */
    MEMORY_TRANSFER_HEADER_SIZE = 3,     /**< Number of bytes in a memory request header. */
    MEMORY_TRANSFER_READ_RESPONSE_PREFIX_SIZE = 2, /**< Number of status bytes before read data. */
    MEMORY_TRANSFER_READ_REQUEST_SIZE = 5,         /**< Encoded size of a memory read request. */
    MEMORY_TRANSFER_MAX_READ_SIZE =
        MEMORY_TRANSFER_MAX_WIRE_SIZE - MEMORY_TRANSFER_READ_RESPONSE_PREFIX_SIZE,
    /**< Maximum read data that fits with the response prefix. */
    MEMORY_TRANSFER_MAX_WRITE_SIZE = MEMORY_TRANSFER_MAX_WIRE_SIZE - MEMORY_TRANSFER_HEADER_SIZE,
    /**< Maximum write data that fits with the request header. */
    MEMORY_TRANSFER_MAX_REQUEST_SIZE =
        MEMORY_TRANSFER_HEADER_SIZE +
        MEMORY_TRANSFER_MAX_WRITE_SIZE, /**< Maximum encoded memory request size. */
};

/**
 * @brief Result of decoding a remote memory response.
 *
 * The result distinguishes accepted and rejected remote operations from malformed responses.
 */
typedef enum {
    MEMORY_TRANSFER_ACCEPTED,         /**< Remote operation was accepted. */
    MEMORY_TRANSFER_REJECTED,         /**< Remote operation was rejected. */
    MEMORY_TRANSFER_INVALID_RESPONSE, /**< Response format or length was invalid. */
} MemoryTransferResult;

/**
 * @brief Encodes a remote memory read request.
 *
 * Writes the group-4 memory header, read flag, offset, and little-endian byte count into output.
 *
 * @param[in] owner Transfer owner identifier.
 * @param[in] offset Remote memory offset.
 * @param[in] length Requested byte count, up to MEMORY_TRANSFER_MAX_READ_SIZE, so the two-byte
 * response prefix remains within MEMORY_TRANSFER_MAX_WIRE_SIZE.
 * @param[out] output Five-byte request payload.
 * @return MEMORY_TRANSFER_READ_REQUEST_SIZE when supported; otherwise zero.
 */
uint8_t memory_transfer_encode_read(uint8_t owner, uint8_t offset, uint16_t length,
                                    uint8_t output[MEMORY_TRANSFER_READ_REQUEST_SIZE]);

/**
 * @brief Encodes a remote memory write request.
 *
 * Writes the group-4 memory header, write flag, offset, and payload bytes into output.
 *
 * @param[in] owner Transfer owner identifier.
 * @param[in] offset Remote memory offset.
 * @param[in] data Bytes to write, or null when length is zero.
 * @param[in] length Byte count to write, up to MEMORY_TRANSFER_MAX_WRITE_SIZE, so the three-byte
 * request header remains within MEMORY_TRANSFER_MAX_WIRE_SIZE.
 * @param[out] output Encoded request payload.
 * @return Encoded byte count, or zero when the payload is invalid or too long.
 */
uint16_t memory_transfer_encode_write(uint8_t owner, uint8_t offset, const uint8_t *data,
                                      uint16_t length,
                                      uint8_t output[MEMORY_TRANSFER_MAX_REQUEST_SIZE]);

/**
 * @brief Decodes a remote memory read response.
 *
 * Validates the accepted status and exact response length, then copies returned bytes after the
 * response prefix into output.
 *
 * @param[in] response Received memory response.
 * @param[in] response_length Received byte count.
 * @param[out] output Destination for the requested bytes, or null when output_length is zero.
 * @param[in] output_length Expected data byte count.
 * @return Accepted, rejected, or invalid-response status.
 */
MemoryTransferResult memory_transfer_decode_read(const uint8_t *response, uint16_t response_length,
                                                 uint8_t *output, uint16_t output_length);

/**
 * @brief Decodes a remote memory write response.
 *
 * Maps response status zero to rejected, status one to accepted, and other responses to invalid.
 *
 * @param[in] response Received memory response.
 * @param[in] response_length Received byte count.
 * @return Accepted, rejected, or invalid-response status.
 */
MemoryTransferResult memory_transfer_decode_write(const uint8_t *response,
                                                  uint16_t response_length);

#endif
