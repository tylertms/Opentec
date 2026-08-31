#ifndef OPENTEC_BASE_MOTOR_COMMAND_PACKET_H
#define OPENTEC_BASE_MOTOR_COMMAND_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_digest.h"

/** @brief Defines fixed motor-command packet sizes and payload limits. */
enum {
    MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE = 5, /**< Size of an acknowledgement, reset, or retry packet. */
    MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE = 11, /**< Size of a digest request packet. */
    MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE = 11, /**< Size of an information request packet. */
    MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE = 1009, /**< Maximum encoded motor-command packet size. */
    MOTOR_COMMAND_PACKET_ENCODING_OVERHEAD = 6, /**< Bytes added around an application payload. */
    MOTOR_COMMAND_PACKET_MAX_PAYLOAD_SIZE =
        MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE - MOTOR_COMMAND_PACKET_ENCODING_OVERHEAD, /**< Maximum application payload size. */
};

/**
 * @brief Encodes an application payload into a motor-command packet.
 *
 * Writes the two-bit mode and sequence header, packet length, unfragmented marker, payload, and checksum
 * to output. An out-of-range sequence is encoded as sequence zero.
 *
 * @param[in] mode Two-bit packet mode value.
 * @param[in] sequence Current sequence value, or an out-of-range value to encode zero.
 * @param[in] adjacent_sequence Previous or next sequence value selected by mode.
 * @param[in] payload Application payload bytes.
 * @param[in] payload_length Number of bytes in payload.
 * @param[out] output Destination for the encoded packet.
 * @param[in] output_capacity Capacity of output in bytes.
 * @param[out] output_length Encoded packet length written on success.
 * @return true when the payload and output storage can hold an encoded packet; otherwise false.
 */
bool motor_command_packet_payload_encode(uint8_t mode, uint8_t sequence, uint8_t adjacent_sequence,
                                         const uint8_t *payload, uint16_t payload_length,
                                         uint8_t *output, uint16_t output_capacity,
                                         uint16_t *output_length);

/**
 * @brief Validates a motor-command packet checksum.
 *
 * Checks the supported packet length range and compares the trailing big-endian checksum with the
 * checksum calculated over the preceding packet bytes.
 *
 * @param[in] input Packet bytes to validate.
 * @param[in] length Number of bytes in input.
 * @return true when input has a supported length and valid checksum; otherwise false.
 */
bool motor_command_packet_checksum_valid(const uint8_t *input, uint16_t length);

/**
 * @brief Encodes a motor-command digest request.
 *
 * Writes the fixed command-7 request with the requested sequence mode and checksum. An out-of-range
 * sequence is encoded as sequence zero.
 *
 * @param[in] sequence Current sequence value, or an out-of-range value to encode zero.
 * @param[in] adjacent_sequence Previous sequence for normal mode or next sequence for retry mode.
 * @param[in] retry Whether to encode retry mode.
 * @param[out] output Destination for the eleven-byte request packet.
 */
void motor_command_packet_digest_request_encode(
    uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE]);

/**
 * @brief Encodes a motor-command information request.
 *
 * Writes the fixed command-5 request for the selected information selector, including its expected
 * response length, sequence mode, and checksum. An out-of-range sequence is encoded as sequence
 * zero.
 *
 * @param[in] selector Information selector from 1 through 9.
 * @param[in] sequence Current sequence value, or an out-of-range value to encode zero.
 * @param[in] adjacent_sequence Previous sequence for normal mode or next sequence for retry mode.
 * @param[in] retry Whether to encode retry mode.
 * @param[out] output Destination for the eleven-byte request packet.
 * @return true when selector is supported; otherwise false.
 */
bool motor_command_packet_info_request_encode(
    uint8_t selector, uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE]);

/**
 * @brief Encodes a motor-command acknowledgement.
 *
 * Writes a five-byte response packet carrying the accepted previous sequence and its checksum.
 *
 * @param[in] previous_sequence Accepted previous sequence value.
 * @param[out] output Destination for the five-byte acknowledgement packet.
 */
void motor_command_packet_acknowledgement_encode(
    uint8_t previous_sequence, uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]);

/**
 * @brief Encodes a motor-command sequence reset.
 *
 * Writes a five-byte response packet carrying the protocol reset flag and its checksum.
 *
 * @param[out] output Destination for the five-byte sequence-reset packet.
 */
void motor_command_packet_sequence_reset_encode(
    uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]);

/**
 * @brief Encodes a motor-command retry response.
 *
 * Writes a five-byte response packet requesting the supplied next sequence and its checksum.
 *
 * @param[in] next_sequence Next sequence value requested from the peer.
 * @param[out] output Destination for the five-byte retry packet.
 */
void motor_command_packet_retry_encode(uint8_t next_sequence,
                                       uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]);

/**
 * @brief Decodes a motor-command digest response.
 *
 * Validates the normal unfragmented packet envelope, command, sequence, length, and checksum before
 * copying its sixteen-byte digest source.
 *
 * @param[in] previous_sequence Expected previous sequence value.
 * @param[in] input Received packet bytes.
 * @param[in] length Number of bytes in input.
 * @param[out] source Destination for the sixteen-byte digest source.
 * @return true when input contains a valid digest response; otherwise false.
 */
bool motor_command_packet_digest_response_decode(uint8_t previous_sequence, const uint8_t *input,
                                                 uint16_t length,
                                                 uint8_t source[MOTOR_COMMAND_DIGEST_SOURCE_SIZE]);

/**
 * @brief Decodes a motor-command information response.
 *
 * Validates the normal unfragmented packet envelope, command, sequence, selector, declared data
 * length, and checksum, then copies the selector-specific response data.
 *
 * @param[in] previous_sequence Expected previous sequence value.
 * @param[in] expected_selector Expected information selector from 1 through 9.
 * @param[in] input Received packet bytes.
 * @param[in] length Number of bytes in input.
 * @param[out] output Destination for selector response data.
 * @param[in] output_capacity Capacity of output in bytes.
 * @return Decoded response length, or zero when input is invalid or storage is insufficient.
 */
uint8_t motor_command_packet_info_response_decode(uint8_t previous_sequence,
                                                  uint8_t expected_selector, const uint8_t *input,
                                                  uint16_t length, uint8_t *output,
                                                  uint8_t output_capacity);

/**
 * @brief Decodes a two-byte motor-command information response.
 *
 * Decodes selector 3 or 4 through the general information-response validator and combines its two
 * data bytes in big-endian order.
 *
 * @param[in] previous_sequence Expected previous sequence value.
 * @param[in] expected_selector Expected information selector, either 3 or 4.
 * @param[in] input Received packet bytes.
 * @param[in] length Number of bytes in input.
 * @param[out] value Destination for the decoded 16-bit value.
 * @return true when input contains the expected two-byte response; otherwise false.
 */
bool motor_command_packet_info_word_response_decode(uint8_t previous_sequence,
                                                    uint8_t expected_selector, const uint8_t *input,
                                                    uint16_t length, uint16_t *value);

#endif
