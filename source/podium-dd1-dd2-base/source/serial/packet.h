#ifndef OPENTEC_BASE_SERIAL_PACKET_H
#define OPENTEC_BASE_SERIAL_PACKET_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Fixed serial packet layout and flag constants. */
enum {
    SERIAL_PACKET_SIZE = 64,                    /**< Total encoded packet size in bytes. */
    SERIAL_PACKET_MAX_PAYLOAD_SIZE = 57,        /**< Maximum payload bytes in one packet. */
    SERIAL_PACKET_START = 0x7b,                 /**< Start boundary byte. */
    SERIAL_PACKET_END = 0x7d,                   /**< End boundary byte. */
    SERIAL_PACKET_TYPE_MASK = 0x0f,             /**< Mask for the packet's logical type. */
    SERIAL_PACKET_FIRST_FRAGMENT = 0x10,        /**< Flag for the first fragment of a message. */
    SERIAL_PACKET_CONTINUATION_FRAGMENT = 0x20, /**< Flag for a continuation fragment. */
    SERIAL_PACKET_FINAL_FRAGMENT = 0x40,        /**< Flag for the final fragment of a message. */
};

/** @brief Result of validating and decoding a serial packet. */
typedef enum {
    SERIAL_PACKET_VALID,            /**< The packet boundaries, length, and checksum are valid. */
    SERIAL_PACKET_INVALID_BOUNDARY, /**< One or both packet boundary bytes are invalid. */
    SERIAL_PACKET_INVALID_LENGTH,   /**< The packet payload length exceeds the protocol limit. */
    SERIAL_PACKET_INVALID_CHECKSUM, /**< The packet checksum does not match its contents. */
} SerialPacketResult;

/** @brief Decoded serial packet fields and payload. */
typedef struct {
    uint8_t type_flags;                              /**< Logical type and fragment flags. */
    uint8_t sequence;                                /**< Transport sequence number. */
    uint8_t payload[SERIAL_PACKET_MAX_PAYLOAD_SIZE]; /**< Decoded payload storage. */
    uint8_t payload_length;                          /**< Number of valid bytes in payload. */
} SerialPacket;

/**
 * @brief Encodes one fixed-size serial packet.
 *
 * Writes packet boundaries, type flags, sequence, payload length, payload, and checksum to output.
 *
 * @param[in] type_flags Logical type and optional fragment flags.
 * @param[in] sequence Transport sequence number.
 * @param[in] payload Payload bytes, or null when payload_length is zero.
 * @param[in] payload_length Number of payload bytes, from zero through
 * SERIAL_PACKET_MAX_PAYLOAD_SIZE.
 * @param[out] output Destination for the encoded packet.
 * @return True when output and payload arguments satisfy the packet limits; otherwise false.
 */
bool serial_packet_encode(uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                          uint8_t payload_length, uint8_t output[SERIAL_PACKET_SIZE]);

/**
 * @brief Encodes one fixed-size serial packet with a one-byte payload.
 *
 * Writes a packet carrying payload without requiring a separate payload pointer.
 *
 * @param[in] type_flags Logical type and optional fragment flags.
 * @param[in] sequence Transport sequence number.
 * @param[in] payload Single payload byte.
 * @param[out] output Destination for the encoded packet.
 * @return True when output is non-null and the packet is encoded; otherwise false.
 */
bool serial_packet_encode_byte(uint8_t type_flags, uint8_t sequence, uint8_t payload,
                               uint8_t output[SERIAL_PACKET_SIZE]);

/**
 * @brief Validates the checksum stored in a fixed-size serial packet.
 *
 * Checks the checksum over the fixed transport region without applying the logical payload-length
 * limit.
 *
 * @param[in] input Received encoded packet.
 * @return True when the stored checksum matches; otherwise false.
 */
bool serial_packet_checksum_valid(const uint8_t input[SERIAL_PACKET_SIZE]);

/**
 * @brief Validates and decodes one fixed-size serial packet.
 *
 * Checks packet boundaries, payload length, and checksum before copying decoded fields into packet.
 *
 * @param[in] input Received encoded packet.
 * @param[out] packet Destination for decoded fields and payload.
 * @return Validation result identifying the first invalid packet condition, or valid.
 */
SerialPacketResult serial_packet_decode(const uint8_t input[SERIAL_PACKET_SIZE],
                                        SerialPacket *packet);

#endif
