#include "transfer/frame.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Internal transfer framing and packed-command constants.
 *
 * These values define reserved markers, bit fields, and validation limits for frame encoding and
 * decoding.
 */
enum {
    TRANSFER_FRAME_ESCAPE = 0x3d,           /**< Escape marker for reserved frame bytes. */
    TRANSFER_FRAME_ESCAPED_START = 0x28,    /**< Escaped suffix representing the start marker. */
    TRANSFER_FRAME_ESCAPED_END = 0x29,      /**< Escaped suffix representing the end marker. */
    TRANSFER_FRAME_MIN_ENCODED_SIZE = 5,    /**< Minimum encoded frame length. */
    TRANSFER_COMMAND_DATA = 1,              /**< Packed command type for data frames. */
    TRANSFER_COMMAND_STATUS = 2,            /**< Packed command type for status frames. */
    TRANSFER_COMMAND_TYPE_SHIFT = 10,       /**< Bit offset of the command type field. */
    TRANSFER_COMMAND_GROUP_SHIFT = 8,       /**< Bit offset of the group field. */
    TRANSFER_COMMAND_SEQUENCE_SHIFT = 3,    /**< Bit offset of sequence or progress fields. */
    TRANSFER_COMMAND_GROUP_MASK = 0x03,     /**< Mask for the two-bit group field. */
    TRANSFER_COMMAND_SEQUENCE_MASK = 0x07,  /**< Mask for the three-bit sequence field. */
    TRANSFER_COMMAND_PROGRESS_MASK = 0x1f,  /**< Mask for the five-bit progress field. */
    TRANSFER_COMMAND_PARAMETER_MASK = 0x07, /**< Mask for the three-bit parameter field. */
};

/**
 * @brief Calculates the transfer protocol CRC-8.
 *
 * Processes each byte least-significant bit first from an all-ones seed with the reflected 0x8c
 * polynomial.
 *
 * @param[in] data Bytes covered by the checksum.
 * @param[in] length Number of bytes to process.
 * @return Eight-bit checksum value.
 */
static uint8_t checksum(const uint8_t *data, uint8_t length) {
    uint8_t crc = UINT8_MAX;
    for (uint8_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (uint8_t)((crc >> 1) ^ 0x8cu) : (uint8_t)(crc >> 1);
        }
    }
    return crc;
}

uint8_t transfer_command_group(uint16_t command) {
    return (uint8_t)(command >> TRANSFER_COMMAND_GROUP_SHIFT) & TRANSFER_COMMAND_GROUP_MASK;
}

uint8_t transfer_command_type(uint16_t command) {
    return (uint8_t)(command >> TRANSFER_COMMAND_TYPE_SHIFT) & TRANSFER_COMMAND_PARAMETER_MASK;
}

uint8_t transfer_command_sequence(uint16_t command) {
    return (uint8_t)(command >> TRANSFER_COMMAND_SEQUENCE_SHIFT) & TRANSFER_COMMAND_SEQUENCE_MASK;
}

uint8_t transfer_command_progress(uint16_t command) {
    return (uint8_t)(command >> TRANSFER_COMMAND_SEQUENCE_SHIFT) & TRANSFER_COMMAND_PROGRESS_MASK;
}

uint8_t transfer_command_parameter(uint16_t command) {
    return (uint8_t)command & TRANSFER_COMMAND_PARAMETER_MASK;
}

uint16_t transfer_empty_command(void) { return 0; }

uint16_t transfer_data_command(uint8_t group, uint8_t sequence, uint8_t parameter) {
    return (uint16_t)TRANSFER_COMMAND_DATA << TRANSFER_COMMAND_TYPE_SHIFT |
           ((uint16_t)group & TRANSFER_COMMAND_GROUP_MASK) << TRANSFER_COMMAND_GROUP_SHIFT |
           ((uint16_t)sequence & TRANSFER_COMMAND_SEQUENCE_MASK)
               << TRANSFER_COMMAND_SEQUENCE_SHIFT |
           ((uint16_t)parameter & TRANSFER_COMMAND_PARAMETER_MASK);
}

uint16_t transfer_status_command(uint8_t group, uint8_t parameter) {
    return (uint16_t)TRANSFER_COMMAND_STATUS << TRANSFER_COMMAND_TYPE_SHIFT |
           ((uint16_t)group & TRANSFER_COMMAND_GROUP_MASK) << TRANSFER_COMMAND_GROUP_SHIFT |
           ((uint16_t)parameter & TRANSFER_COMMAND_PARAMETER_MASK);
}

uint16_t transfer_progress_command(uint8_t group, uint8_t parameter, uint8_t sequence) {
    return (uint16_t)TRANSFER_COMMAND_STATUS << TRANSFER_COMMAND_TYPE_SHIFT |
           ((uint16_t)group & TRANSFER_COMMAND_GROUP_MASK) << TRANSFER_COMMAND_GROUP_SHIFT |
           ((uint16_t)sequence & TRANSFER_COMMAND_PROGRESS_MASK)
               << TRANSFER_COMMAND_SEQUENCE_SHIFT |
           ((uint16_t)parameter & TRANSFER_COMMAND_PARAMETER_MASK);
}

/**
 * @brief Appends one transfer byte with reserved-marker escaping.
 *
 * Doubles the escape marker and maps the start and end markers to their escaped suffix values.
 *
 * @param[in] value Decoded byte to append.
 * @param[out] output Encoded frame buffer receiving one or two bytes.
 * @param[in] output_length Current encoded length and insertion offset.
 * @return Encoded length after appending the byte.
 */
static uint16_t append_encoded(uint8_t value, uint8_t *output, uint16_t output_length) {
    if (value == TRANSFER_FRAME_ESCAPE) {
        output[output_length++] = TRANSFER_FRAME_ESCAPE;
        output[output_length++] = TRANSFER_FRAME_ESCAPE;
    } else if (value == TRANSFER_FRAME_START) {
        output[output_length++] = TRANSFER_FRAME_ESCAPE;
        output[output_length++] = TRANSFER_FRAME_ESCAPED_START;
    } else if (value == TRANSFER_FRAME_END) {
        output[output_length++] = TRANSFER_FRAME_ESCAPE;
        output[output_length++] = TRANSFER_FRAME_ESCAPED_END;
    } else {
        output[output_length++] = value;
    }
    return output_length;
}

uint16_t transfer_frame_encode_values(uint16_t command, const uint8_t *payload,
                                      uint8_t payload_length,
                                      uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]) {
    if (payload_length > TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE ||
        (payload_length != 0 && payload == NULL)) {
        return 0;
    }

    uint8_t decoded[TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE + 2];
    decoded[0] = (uint8_t)(command >> 8);
    decoded[1] = (uint8_t)command;
    for (uint8_t index = 0; index < payload_length; index++) {
        decoded[index + 2] = payload[index];
    }

    uint16_t output_length = 1;
    output[0] = TRANSFER_FRAME_START;
    uint8_t decoded_length = payload_length + 2;
    for (uint8_t index = 0; index < decoded_length; index++) {
        output_length = append_encoded(decoded[index], output, output_length);
    }
    output_length = append_encoded(checksum(decoded, decoded_length), output, output_length);
    output[output_length++] = TRANSFER_FRAME_END;
    return output_length;
}

uint16_t transfer_frame_encode(const TransferFrame *frame,
                               uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]) {
    return transfer_frame_encode_values(frame->command, frame->payload, frame->payload_length,
                                        output);
}

/**
 * @brief Decodes one suffix that follows the transfer escape marker.
 *
 * Accepts the doubled escape suffix and the mapped start and end suffixes.
 *
 * @param[in] value Escaped suffix byte.
 * @return Decoded reserved byte, or a value above eight bits for an invalid suffix.
 */
static uint16_t decode_byte(uint8_t value) {
    if (value == TRANSFER_FRAME_ESCAPE) {
        return TRANSFER_FRAME_ESCAPE;
    }
    if (value == TRANSFER_FRAME_ESCAPED_START) {
        return TRANSFER_FRAME_START;
    }
    if (value == TRANSFER_FRAME_ESCAPED_END) {
        return TRANSFER_FRAME_END;
    }
    return UINT16_MAX;
}

TransferFrameResult transfer_frame_decode(const uint8_t *input, uint16_t input_length,
                                          TransferFrame *frame) {
    if (input_length < TRANSFER_FRAME_MIN_ENCODED_SIZE ||
        input_length > TRANSFER_FRAME_MAX_RECEIVED_SIZE) {
        return TRANSFER_FRAME_INVALID_LENGTH;
    }
    if (input[0] != TRANSFER_FRAME_START || input[input_length - 1] != TRANSFER_FRAME_END) {
        return TRANSFER_FRAME_INVALID_BOUNDARY;
    }

    uint8_t decoded[TRANSFER_FRAME_MAX_PAYLOAD_SIZE + 3];
    uint8_t decoded_length = 0;
    for (uint16_t index = 1; index < input_length - 1; index++) {
        uint8_t value = input[index];
        if (value == TRANSFER_FRAME_ESCAPE) {
            index++;
            if (index >= input_length - 1) {
                return TRANSFER_FRAME_INVALID_ESCAPE;
            }
            uint16_t escaped_value = decode_byte(input[index]);
            if (escaped_value > UINT8_MAX) {
                return TRANSFER_FRAME_INVALID_ESCAPE;
            }
            value = (uint8_t)escaped_value;
        }
        if (decoded_length == sizeof(decoded)) {
            return TRANSFER_FRAME_INVALID_LENGTH;
        }
        decoded[decoded_length++] = value;
    }

    if (decoded_length < 3 || decoded_length - 3 > TRANSFER_FRAME_MAX_PAYLOAD_SIZE) {
        return TRANSFER_FRAME_INVALID_LENGTH;
    }
    if (checksum(decoded, decoded_length - 1) != decoded[decoded_length - 1]) {
        return TRANSFER_FRAME_INVALID_CHECKSUM;
    }

    frame->command = (uint16_t)decoded[0] << 8 | decoded[1];
    frame->payload_length = decoded_length - 3;
    for (uint8_t index = 0; index < frame->payload_length; index++) {
        frame->payload[index] = decoded[index + 2];
    }
    return TRANSFER_FRAME_VALID;
}
