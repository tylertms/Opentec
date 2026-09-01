#ifndef OPENTEC_BASE_PEDAL_FRAME_H
#define OPENTEC_BASE_PEDAL_FRAME_H

#include <stdint.h>

/**
 * @brief Fixed sizes and markers for framed pedal messages.
 */
enum {
    PEDAL_FRAME_SIZE = 12,        /**< Complete encoded frame size in bytes. */
    PEDAL_FRAME_PAYLOAD_SIZE = 8, /**< Payload size in bytes. */
    PEDAL_FRAME_START = 0x7b,     /**< Frame start marker. */
    PEDAL_FRAME_END = 0x7d,       /**< Frame end marker. */
};

/**
 * @brief Identifies the result of decoding a framed pedal message.
 */
typedef enum {
    PEDAL_FRAME_VALID,            /**< Frame markers and checksum are valid. */
    PEDAL_FRAME_INVALID_BOUNDARY, /**< Frame start or end marker is invalid. */
    PEDAL_FRAME_INVALID_CHECKSUM, /**< Frame checksum is invalid. */
} PedalFrameResult;

/**
 * @brief Stores the type and payload of a framed pedal message.
 */
typedef struct {
    uint8_t type;                              /**< Message type byte. */
    uint8_t payload[PEDAL_FRAME_PAYLOAD_SIZE]; /**< Message payload bytes. */
} PedalFrame;

/**
 * @brief Encodes a pedal message into its fixed wire frame.
 *
 * Writes markers, type, payload, and checksum into output in wire order.
 *
 * @param[in] frame Message type and payload to encode.
 * @param[out] output Destination for the complete encoded frame.
 */
void pedal_frame_encode(const PedalFrame *frame, uint8_t output[PEDAL_FRAME_SIZE]);

/**
 * @brief Validates and decodes a fixed-size pedal wire frame.
 *
 * Checks both boundary markers and the checksum before writing the decoded type and payload.
 *
 * @param[in] input Complete encoded pedal frame.
 * @param[out] frame Destination for the decoded message.
 * @return Frame validity result.
 */
PedalFrameResult pedal_frame_decode(const uint8_t input[PEDAL_FRAME_SIZE], PedalFrame *frame);

#endif
