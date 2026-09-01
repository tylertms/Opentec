#ifndef OPENTEC_COMMON_WQR_FRAME_H
#define OPENTEC_COMMON_WQR_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Fixed sizes and wire values used by WQR frames. */
enum {
    WQR_FRAME_SIZE = 64,         /**< Total number of bytes in one WQR frame. */
    WQR_FRAME_BODY_SIZE = 60,    /**< Number of bytes covered by the frame CRC. */
    WQR_FRAME_PAYLOAD_SIZE = 57, /**< Maximum payload bytes carried by one WQR frame. */
    WQR_FRAME_TYPE_MASK = 0x0f,  /**< Mask selecting the payload type bits from the type byte. */
    WQR_FRAME_FIRST = 0x10,      /**< Flag marking the first fragment of a multi-frame payload. */
    WQR_FRAME_MORE = 0x20,       /**< Flag marking a middle fragment of a multi-frame payload. */
    WQR_FRAME_LAST = 0x40,       /**< Flag marking the final fragment of a multi-frame payload. */
    WQR_FRAME_FRAGMENT_MASK =
        0x70,                    /**< Mask selecting the fragmentation flags from the type byte. */
    WQR_FRAME_NACK = 0,          /**< Control-frame type for a negative acknowledgement. */
    WQR_FRAME_ACK = 1,           /**< Control-frame type for a positive acknowledgement. */
    WQR_PAYLOAD_PRIMARY_SPI = 2, /**< Payload type for the primary byte-oriented SPI transport. */
    WQR_PAYLOAD_ALTERNATE_SPI =
        3,                 /**< Payload type for the alternate word-oriented SPI transport. */
    WQR_PAYLOAD_I2C = 4,   /**< Payload type for an I2C transfer request. */
    WQR_PAYLOAD_STATUS = 5 /**< Payload type for a runtime status request. */
};

/**
 * @brief Decoded view of one valid WQR frame.
 *
 * The payload pointer refers to storage owned by the parsed frame and remains valid only while that
 * frame buffer is unchanged and available.
 */
typedef struct {
    const uint8_t *payload; /**< Pointer to the frame payload bytes. */
    size_t payload_length;  /**< Number of valid bytes available through payload. */
    uint8_t type_flags;     /**< Payload type and fragmentation flags from the frame. */
    uint8_t sequence;       /**< Sequence number carried by the frame. */
} wqr_frame_view;

/**
 * @brief Calculates the WQR frame CRC.
 *
 * Applies the reflected CRC-16 polynomial `0x8408` with an initial value of zero across the
 * requested byte range.
 *
 * @param[in] data Bytes to include in the calculation.
 * @param[in] length Number of bytes to process.
 * @return Calculated 16-bit CRC.
 */
uint16_t wqr_frame_crc(const uint8_t *data, size_t length);

/**
 * @brief Builds one complete WQR transport frame.
 *
 * Clears the destination, writes framing metadata and payload, and appends the CRC and end marker.
 * Rejects null destinations, oversized payloads, and null nonempty payloads.
 *
 * @param[out] frame Complete 64-byte destination frame.
 * @param[in] type_flags Payload type and fragmentation flags.
 * @param[in] sequence Frame sequence value.
 * @param[in] payload Payload bytes, or null when the payload length is zero.
 * @param[in] payload_length Number of payload bytes to encode.
 * @return True when a valid frame was built.
 */
bool wqr_frame_build(uint8_t frame[WQR_FRAME_SIZE], uint8_t type_flags, uint8_t sequence,
                     const uint8_t *payload, size_t payload_length);

/**
 * @brief Validates and decodes one complete WQR transport frame.
 *
 * Verifies boundary markers, payload length, and CRC before publishing a view into the supplied
 * frame buffer.
 *
 * @param[in] frame Complete 64-byte source frame.
 * @param[out] view Decoded metadata and payload view.
 * @return True when the frame is structurally valid and has a matching CRC.
 */
bool wqr_frame_parse(const uint8_t frame[WQR_FRAME_SIZE], wqr_frame_view *view);

#endif
