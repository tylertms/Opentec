#include "wheel/memory_transfer.h"

#include <stdint.h>

enum {
    WHEEL_MEMORY_TRANSFER_CONTROL_COMMAND = 2,
    WHEEL_MEMORY_TRANSFER_READ_FLAG = 1,
    WHEEL_MEMORY_TRANSFER_RESPONSE_REJECTED = 0,
    WHEEL_MEMORY_TRANSFER_RESPONSE_ACCEPTED = 1,
    WHEEL_MEMORY_TRANSFER_READ_DATA_OFFSET = 2,
};

/**
 * @brief Encodes an attached-wheel memory control header.
 *
 * Packs the control command, shifted owner identifier, direction bit, and remote offset.
 *
 * @param[in] owner Transfer owner identifier.
 * @param[in] direction Zero for writes or one for reads.
 * @param[in] offset Attached-wheel memory offset.
 * @param[out] output Three-byte control header.
 */
static void encode_header(uint8_t owner, uint8_t direction, uint8_t offset, uint8_t *output) {
    output[0] = WHEEL_MEMORY_TRANSFER_CONTROL_COMMAND;
    output[1] = (uint8_t)((owner << 1) | direction);
    output[2] = offset;
}

/**
 * @brief Encodes an attached-wheel memory read request.
 *
 * Writes the control command, owner and read flag, memory offset, and little-endian transfer count.
 *
 * @param[in] owner Transfer owner identifier.
 * @param[in] offset Attached-wheel memory offset.
 * @param[in] length Requested byte count.
 * @param[out] output Five-byte request payload.
 * @return Five when the requested count fits one response frame; otherwise zero.
 */
uint8_t wheel_memory_transfer_encode_read(uint8_t owner, uint8_t offset, uint16_t length,
                                          uint8_t output[WHEEL_MEMORY_TRANSFER_READ_REQUEST_SIZE]) {
    if (length > WHEEL_MEMORY_TRANSFER_MAX_READ_SIZE) {
        return 0;
    }
    encode_header(owner, WHEEL_MEMORY_TRANSFER_READ_FLAG, offset, output);
    output[3] = (uint8_t)length;
    output[4] = (uint8_t)(length >> 8);
    return WHEEL_MEMORY_TRANSFER_READ_REQUEST_SIZE;
}

/**
 * @brief Encodes an attached-wheel memory write request.
 *
 * Writes the control command, owner and write flag, memory offset, and payload into one transport
 * frame.
 *
 * @param[in] owner Transfer owner identifier.
 * @param[in] offset Attached-wheel memory offset.
 * @param[in] data Bytes to write, or null when length is zero.
 * @param[in] length Byte count to write.
 * @param[out] output Encoded request payload.
 * @return Encoded byte count, or zero when the payload is invalid or too long.
 */
uint8_t wheel_memory_transfer_encode_write(uint8_t owner, uint8_t offset, const uint8_t *data,
                                           uint8_t length,
                                           uint8_t output[WHEEL_TRANSPORT_PAYLOAD_SIZE]) {
    if (length > WHEEL_MEMORY_TRANSFER_MAX_WRITE_SIZE || (data == 0 && length != 0)) {
        return 0;
    }
    encode_header(owner, 0, offset, output);
    for (uint8_t index = 0; index < length; index++) {
        output[WHEEL_MEMORY_TRANSFER_HEADER_SIZE + index] = data[index];
    }
    return WHEEL_MEMORY_TRANSFER_HEADER_SIZE + length;
}

/**
 * @brief Decodes an attached-wheel memory response status.
 *
 * Maps status zero to rejected, status one to accepted, and every other form to invalid.
 *
 * @param[in] response Received memory response.
 * @param[in] response_length Received byte count.
 * @return Accepted, rejected, or invalid-response status.
 */
static WheelMemoryTransferResult decode_status(const uint8_t *response, uint8_t response_length) {
    if (response == 0 || response_length == 0) {
        return WHEEL_MEMORY_TRANSFER_INVALID_RESPONSE;
    }
    if (response[0] == WHEEL_MEMORY_TRANSFER_RESPONSE_REJECTED) {
        return WHEEL_MEMORY_TRANSFER_REJECTED;
    }
    return response[0] == WHEEL_MEMORY_TRANSFER_RESPONSE_ACCEPTED
               ? WHEEL_MEMORY_TRANSFER_ACCEPTED
               : WHEEL_MEMORY_TRANSFER_INVALID_RESPONSE;
}

/**
 * @brief Decodes an attached-wheel memory read response.
 *
 * Accepts status one, skips the two-byte response prefix, and copies the requested data only when
 * the response length is exact.
 *
 * @param[in] response Received memory response.
 * @param[in] response_length Received byte count.
 * @param[out] output Destination for the requested bytes.
 * @param[in] output_length Expected data byte count.
 * @return Accepted, rejected, or invalid-response status.
 */
WheelMemoryTransferResult wheel_memory_transfer_decode_read(const uint8_t *response,
                                                            uint8_t response_length,
                                                            uint8_t *output,
                                                            uint8_t output_length) {
    WheelMemoryTransferResult result = decode_status(response, response_length);
    if (result != WHEEL_MEMORY_TRANSFER_ACCEPTED) {
        return result;
    }
    if (output == 0 || response_length != output_length + WHEEL_MEMORY_TRANSFER_READ_DATA_OFFSET) {
        return WHEEL_MEMORY_TRANSFER_INVALID_RESPONSE;
    }
    for (uint8_t index = 0; index < output_length; index++) {
        output[index] = response[WHEEL_MEMORY_TRANSFER_READ_DATA_OFFSET + index];
    }
    return WHEEL_MEMORY_TRANSFER_ACCEPTED;
}

/**
 * @brief Decodes an attached-wheel memory write response.
 *
 * Maps response status zero to rejected and status one to accepted.
 *
 * @param[in] response Received memory response.
 * @param[in] response_length Received byte count.
 * @return Accepted, rejected, or invalid-response status.
 */
WheelMemoryTransferResult wheel_memory_transfer_decode_write(const uint8_t *response,
                                                             uint8_t response_length) {
    return decode_status(response, response_length);
}
