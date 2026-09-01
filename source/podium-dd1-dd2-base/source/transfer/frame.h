#ifndef PODIUM_DD1_DD2_BASE_TRANSFER_FRAME_H
#define PODIUM_DD1_DD2_BASE_TRANSFER_FRAME_H

#include <stdint.h>

/**
 * @brief Transfer-frame wire-format limits and boundary markers.
 *
 * Encoded frames use reserved start and end markers, while separate send and receive limits
 * account for the protocol's framing requirements.
 */
enum {
    TRANSFER_FRAME_START = 0x3c,                /**< Start marker for an encoded transfer frame. */
    TRANSFER_FRAME_END = 0x3e,                  /**< End marker for an encoded transfer frame. */
    TRANSFER_FRAME_MAX_PAYLOAD_SIZE = 125,      /**< Maximum decoded payload capacity. */
    TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE = 124, /**< Maximum payload accepted by the encoder. */
    TRANSFER_FRAME_MAX_RECEIVED_SIZE = 135,     /**< Maximum decoded transfer-frame size. */
    TRANSFER_FRAME_MAX_ENCODED_SIZE = 256, /**< Capacity required for an encoded frame buffer. */
};

/**
 * @brief Decoded transfer command and payload.
 *
 * The command identifies the transfer operation and the payload contains its decoded data bytes.
 */
typedef struct {
    uint16_t command;                                 /**< Packed transfer command value. */
    uint8_t payload[TRANSFER_FRAME_MAX_PAYLOAD_SIZE]; /**< Decoded payload bytes. */
    uint8_t payload_length;                           /**< Number of valid bytes in payload. */
} TransferFrame;

/**
 * @brief Result of transfer-frame validation and decoding.
 *
 * The invalid values identify the first framing or integrity check that rejected an encoded
 * frame.
 */
typedef enum {
    TRANSFER_FRAME_VALID,            /**< Frame decoded successfully. */
    TRANSFER_FRAME_INVALID_LENGTH,   /**< Frame length is outside the supported range. */
    TRANSFER_FRAME_INVALID_BOUNDARY, /**< Start or end marker is incorrect. */
    TRANSFER_FRAME_INVALID_ESCAPE,   /**< Escape marker has an invalid or missing suffix. */
    TRANSFER_FRAME_INVALID_CHECKSUM, /**< Decoded frame checksum does not match. */
} TransferFrameResult;

/**
 * @brief Extracts the transfer group from a packed command.
 *
 * Reads the command's two-bit group field without changing the packed command.
 *
 * @param[in] command Packed transfer command.
 * @return Group value from zero through three.
 */
uint8_t transfer_command_group(uint16_t command);

/**
 * @brief Extracts the command type from a packed command.
 *
 * Reads the command's three-bit type field without changing the packed command.
 *
 * @param[in] command Packed transfer command.
 * @return Command type value from zero through seven.
 */
uint8_t transfer_command_type(uint16_t command);

/**
 * @brief Extracts the data sequence from a packed command.
 *
 * Reads the command's three-bit sequence field used by data commands.
 *
 * @param[in] command Packed transfer command.
 * @return Sequence value from zero through seven.
 */
uint8_t transfer_command_sequence(uint16_t command);

/**
 * @brief Extracts progress or error information from a packed command.
 *
 * Reads the command's five-bit progress field used by status and error commands.
 *
 * @param[in] command Packed transfer command.
 * @return Progress or error value from zero through 31.
 */
uint8_t transfer_command_progress(uint16_t command);

/**
 * @brief Extracts the fragment parameter from a packed command.
 *
 * Reads the command's three-bit parameter field.
 *
 * @param[in] command Packed transfer command.
 * @return Parameter value from zero through seven.
 */
uint8_t transfer_command_parameter(uint16_t command);

/**
 * @brief Builds the empty transfer command.
 *
 * Returns the zero command used when no transfer action is present.
 *
 * @return Zero-valued command.
 */
uint16_t transfer_empty_command(void);

/**
 * @brief Packs a transfer data command.
 *
 * Combines the data type with its group, sequence, and fragment parameter fields.
 *
 * @param[in] group Two-bit transfer group.
 * @param[in] sequence Three-bit data sequence.
 * @param[in] parameter Three-bit fragment parameter.
 * @return Packed command value.
 */
uint16_t transfer_data_command(uint8_t group, uint8_t sequence, uint8_t parameter);

/**
 * @brief Packs a transfer acknowledgement command.
 *
 * Combines the status type with its group and acknowledged fragment parameter fields.
 *
 * @param[in] group Two-bit transfer group.
 * @param[in] parameter Three-bit acknowledged fragment parameter.
 * @return Packed command value.
 */
uint16_t transfer_status_command(uint8_t group, uint8_t parameter);

/**
 * @brief Packs a transfer progress or error command.
 *
 * Combines the status type with its group, progress, and fragment parameter fields.
 *
 * @param[in] group Two-bit transfer group.
 * @param[in] parameter Three-bit fragment parameter.
 * @param[in] sequence Five-bit progress or error value.
 * @return Packed command value.
 */
uint16_t transfer_progress_command(uint8_t group, uint8_t parameter, uint8_t sequence);

/**
 * @brief Encodes command values, payload, checksum, and framing markers.
 *
 * Escapes reserved marker bytes and surrounds the command, payload, and checksum with frame
 * boundaries.
 *
 * @param[in] command Packed transfer command.
 * @param[in] payload Payload bytes, or null when payload_length is zero.
 * @param[in] payload_length Payload length from zero through 124 bytes.
 * @param[out] output Destination with room for TRANSFER_FRAME_MAX_ENCODED_SIZE bytes.
 * @return Encoded byte count, or zero when the payload is invalid or too long.
 */
uint16_t transfer_frame_encode_values(uint16_t command, const uint8_t *payload,
                                      uint8_t payload_length,
                                      uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]);

/**
 * @brief Encodes one decoded transfer frame.
 *
 * Encodes the frame command and payload with a checksum, escaped markers, and frame boundaries.
 *
 * @param[in] frame Command and payload to encode; payload_length must not exceed 124 bytes.
 * @param[out] output Destination with room for TRANSFER_FRAME_MAX_ENCODED_SIZE bytes.
 * @return Encoded byte count, or zero when the payload is too long.
 */
uint16_t transfer_frame_encode(const TransferFrame *frame,
                               uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]);

/**
 * @brief Validates and decodes one escaped transfer frame.
 *
 * Validates boundaries, escaping, decoded length, and checksum before publishing the command and
 * payload to frame.
 *
 * @param[in] input Complete encoded frame including boundary markers.
 * @param[in] input_length Encoded byte count from 5 through 256.
 * @param[out] frame Destination for the decoded command and payload.
 * @return Frame status identifying length, boundary, escape, or checksum failures.
 */
TransferFrameResult transfer_frame_decode(const uint8_t *input, uint16_t input_length,
                                          TransferFrame *frame);

#endif
