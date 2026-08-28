#include "wheel/status_memory_packet.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_STATUS_MEMORY_SEQUENCE_MASK = 3,
    WHEEL_STATUS_MEMORY_SEQUENCE_SHIFT = 2,
    WHEEL_STATUS_MEMORY_MODE_SHIFT = 5,
    WHEEL_STATUS_MEMORY_RESPONSE_FLAG = 0x80,
    WHEEL_STATUS_MEMORY_RESET_FLAG = 0x40,
    WHEEL_STATUS_MEMORY_MODE_MASK = 0x60,
    WHEEL_STATUS_MEMORY_DIGEST_COMMAND = 7,
    WHEEL_STATUS_MEMORY_DIGEST_LENGTH = 20,
    WHEEL_STATUS_MEMORY_COMMAND_BODY_LENGTH = 6,
    WHEEL_STATUS_MEMORY_DIGEST_RESPONSE_COMMAND = 0x87,
    WHEEL_STATUS_MEMORY_FRAGMENT_MASK = 7,
    WHEEL_STATUS_MEMORY_FRAGMENT_OFFSET = 3,
    WHEEL_STATUS_MEMORY_COMMAND_OFFSET = 4,
    WHEEL_STATUS_MEMORY_DIGEST_SOURCE_OFFSET = 9,
    WHEEL_STATUS_MEMORY_CHECKSUM_SIZE = 2,
    WHEEL_STATUS_MEMORY_MIN_PACKET_SIZE = 3,
};

/**
 * @brief Updates the wheel-status packet checksum.
 *
 * Applies the nibble-reduced CRC-16 recurrence to one packet byte.
 *
 * @param[in] checksum Current checksum value.
 * @param[in] data Next packet byte.
 * @return Updated checksum value.
 */
static uint16_t update_checksum(uint16_t checksum, uint8_t data) {
    uint16_t mixed = (uint8_t)data ^ checksum;
    uint16_t low = (uint8_t)mixed;
    uint16_t nibble = mixed & 0x0f;
    mixed = low ^ (nibble << 4);
    uint16_t high = mixed >> 4;
    uint16_t result = (checksum >> 8) ^ high;
    return result ^ ((((mixed + mixed) ^ high) << 4) ^ nibble) << 3;
}

/**
 * @brief Calculates a wheel-status packet checksum.
 *
 * Starts at zero and folds the selected packet prefix into a 16-bit checksum.
 *
 * @param[in] data Packet prefix to checksum.
 * @param[in] length Prefix byte count.
 * @return Calculated checksum value.
 */
static uint16_t checksum(const uint8_t *data, uint8_t length) {
    uint16_t value = 0;
    for (uint8_t index = 0; index < length; index++) {
        value = update_checksum(value, data[index]);
    }
    return value;
}

/**
 * @brief Validates a wheel-status packet checksum.
 *
 * Rejects packets outside the supported mailbox size and compares the trailing checksum in
 * high-byte-first order against the checksum of all preceding bytes.
 *
 * @param[in] input Received packet.
 * @param[in] length Received byte count.
 * @return True when the packet length and checksum are valid.
 */
static bool checksum_valid(const uint8_t *input, uint16_t length) {
    if (input == 0 || length < WHEEL_STATUS_MEMORY_MIN_PACKET_SIZE ||
        length > WHEEL_STATUS_MEMORY_MAX_PACKET_SIZE) {
        return false;
    }
    uint16_t value = 0;
    for (uint16_t index = 0; index < length - WHEEL_STATUS_MEMORY_CHECKSUM_SIZE; index++) {
        value = update_checksum(value, input[index]);
    }
    return input[length - 2] == (uint8_t)(value >> 8) && input[length - 1] == (uint8_t)value;
}

/**
 * @brief Appends a wheel-status packet checksum.
 *
 * Calculates the selected body checksum and writes it high byte first after the body.
 *
 * @param[in,out] output Packet body and checksum destination.
 * @param[in] body_length Packet body byte count.
 */
static void finish(uint8_t *output, uint8_t body_length) {
    uint16_t value = checksum(output, body_length);
    output[body_length] = (uint8_t)(value >> 8);
    output[body_length + 1] = (uint8_t)value;
}

/**
 * @brief Encodes a wheel-status digest request.
 *
 * Builds command 7 with its fixed body length and digest length, normal or retry sequencing, and
 * a big-endian checksum over the first nine bytes.
 *
 * @param[in] sequence Current two-bit sequence, or an out-of-range value to reset it to zero.
 * @param[in] adjacent_sequence Previous sequence for normal mode or next sequence for retry mode.
 * @param[in] retry Selects retry header mode when true.
 * @param[out] output Eleven-byte digest request.
 */
void wheel_status_memory_digest_request_encode(
    uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[WHEEL_STATUS_MEMORY_DIGEST_REQUEST_SIZE]) {
    for (uint8_t index = 0; index < WHEEL_STATUS_MEMORY_DIGEST_REQUEST_SIZE; index++) {
        output[index] = 0;
    }
    if (sequence > WHEEL_STATUS_MEMORY_SEQUENCE_MASK) {
        sequence = 0;
    }
    output[0] = (adjacent_sequence & WHEEL_STATUS_MEMORY_SEQUENCE_MASK) |
                (sequence << WHEEL_STATUS_MEMORY_SEQUENCE_SHIFT) |
                ((uint8_t)retry << WHEEL_STATUS_MEMORY_MODE_SHIFT);
    output[2] = WHEEL_STATUS_MEMORY_COMMAND_BODY_LENGTH;
    output[4] = WHEEL_STATUS_MEMORY_DIGEST_COMMAND;
    output[8] = WHEEL_STATUS_MEMORY_DIGEST_LENGTH;
    finish(output, WHEEL_STATUS_MEMORY_DIGEST_REQUEST_SIZE - 2);
}

/**
 * @brief Encodes a wheel-status acknowledgement.
 *
 * Builds a response header carrying the previous sequence and appends a big-endian checksum over
 * its three-byte body.
 *
 * @param[in] previous_sequence Previous two-bit sequence.
 * @param[out] output Five-byte acknowledgement packet.
 */
void wheel_status_memory_acknowledgement_encode(
    uint8_t previous_sequence, uint8_t output[WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE]) {
    output[0] =
        WHEEL_STATUS_MEMORY_RESPONSE_FLAG | (previous_sequence & WHEEL_STATUS_MEMORY_SEQUENCE_MASK);
    output[1] = 0;
    output[2] = 0;
    finish(output, WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE - 2);
}

/**
 * @brief Encodes a wheel-status sequence reset.
 *
 * Builds the response and reset flags with an empty body and appends a big-endian checksum over
 * the first three bytes.
 *
 * @param[out] output Five-byte sequence-reset packet.
 */
void wheel_status_memory_sequence_reset_encode(
    uint8_t output[WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE]) {
    output[0] = WHEEL_STATUS_MEMORY_RESPONSE_FLAG | WHEEL_STATUS_MEMORY_RESET_FLAG;
    output[1] = 0;
    output[2] = 0;
    finish(output, WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE - 2);
}

/**
 * @brief Decodes a wheel-status digest response.
 *
 * Accepts a checksum-valid, unfragmented command 0x87 carrying the expected prior sequence and
 * copies its sixteen-byte digest source.
 *
 * @param[in] previous_sequence Expected prior two-bit sequence.
 * @param[in] input Received wheel-status packet.
 * @param[in] length Received byte count.
 * @param[out] source Sixteen-byte digest source.
 * @return True when the packet contains a valid digest response.
 */
bool wheel_status_memory_digest_response_decode(uint8_t previous_sequence, const uint8_t *input,
                                                uint16_t length,
                                                uint8_t source[WHEEL_STATUS_DIGEST_SOURCE_SIZE]) {
    if (source == 0 ||
        length < WHEEL_STATUS_MEMORY_DIGEST_SOURCE_OFFSET + WHEEL_STATUS_DIGEST_SOURCE_SIZE +
                     WHEEL_STATUS_MEMORY_CHECKSUM_SIZE ||
        !checksum_valid(input, length) ||
        (input[0] & (WHEEL_STATUS_MEMORY_RESPONSE_FLAG | WHEEL_STATUS_MEMORY_MODE_MASK)) != 0 ||
        (input[0] & WHEEL_STATUS_MEMORY_SEQUENCE_MASK) !=
            (previous_sequence & WHEEL_STATUS_MEMORY_SEQUENCE_MASK) ||
        (input[WHEEL_STATUS_MEMORY_FRAGMENT_OFFSET] & WHEEL_STATUS_MEMORY_FRAGMENT_MASK) != 0 ||
        input[WHEEL_STATUS_MEMORY_COMMAND_OFFSET] != WHEEL_STATUS_MEMORY_DIGEST_RESPONSE_COMMAND) {
        return false;
    }
    for (uint8_t index = 0; index < WHEEL_STATUS_DIGEST_SOURCE_SIZE; index++) {
        source[index] = input[WHEEL_STATUS_MEMORY_DIGEST_SOURCE_OFFSET + index];
    }
    return true;
}
