#include "motor/command_packet.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    MOTOR_COMMAND_PACKET_SEQUENCE_MASK = 3,
    MOTOR_COMMAND_PACKET_SEQUENCE_SHIFT = 2,
    MOTOR_COMMAND_PACKET_MODE_SHIFT = 5,
    MOTOR_COMMAND_PACKET_RESPONSE_FLAG = 0x80,
    MOTOR_COMMAND_PACKET_RESET_FLAG = 0x40,
    MOTOR_COMMAND_PACKET_MODE_MASK = 0x60,
    MOTOR_COMMAND_PACKET_DIGEST_COMMAND = 7,
    MOTOR_COMMAND_PACKET_INFO_COMMAND = 5,
    MOTOR_COMMAND_PACKET_DIGEST_LENGTH = 20,
    MOTOR_COMMAND_PACKET_COMMAND_BODY_LENGTH = 6,
    MOTOR_COMMAND_PACKET_DIGEST_RESPONSE_COMMAND = 0x87,
    MOTOR_COMMAND_PACKET_INFO_RESPONSE_COMMAND = 0x85,
    MOTOR_COMMAND_PACKET_FRAGMENT_MASK = 7,
    MOTOR_COMMAND_PACKET_FRAGMENT_OFFSET = 3,
    MOTOR_COMMAND_PACKET_COMMAND_OFFSET = 4,
    MOTOR_COMMAND_PACKET_SELECTOR_OFFSET = 6,
    MOTOR_COMMAND_PACKET_DIGEST_SOURCE_OFFSET = 9,
    MOTOR_COMMAND_PACKET_INFO_SOURCE_OFFSET = 9,
    MOTOR_COMMAND_PACKET_CHECKSUM_SIZE = 2,
    MOTOR_COMMAND_PACKET_MIN_PACKET_SIZE = 3,
};

static const uint8_t info_lengths[] = {0, 2, 20, 2, 2, 1, 2, 16, 4, 50};

/**
 * @brief Updates the motor-command packet checksum.
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
 * @brief Calculates a motor-command packet checksum.
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
 * @brief Validates a motor-command packet checksum.
 *
 * Rejects packets outside the supported mailbox size and compares the trailing checksum in
 * high-byte-first order against the checksum of all preceding bytes.
 *
 * @param[in] input Received packet.
 * @param[in] length Received byte count.
 * @return True when the packet length and checksum are valid.
 */
static bool checksum_valid(const uint8_t *input, uint16_t length) {
    if (input == 0 || length < MOTOR_COMMAND_PACKET_MIN_PACKET_SIZE ||
        length > MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE) {
        return false;
    }
    uint16_t value = 0;
    for (uint16_t index = 0; index < length - MOTOR_COMMAND_PACKET_CHECKSUM_SIZE; index++) {
        value = update_checksum(value, input[index]);
    }
    return input[length - 2] == (uint8_t)(value >> 8) && input[length - 1] == (uint8_t)value;
}

/**
 * @brief Validates a normal motor-command envelope.
 *
 * Checks the packet checksum, declared and received lengths, normal-mode header, prior sequence,
 * unfragmented marker, command, and caller-selected minimum length.
 *
 * @param[in] previous_sequence Expected prior two-bit sequence.
 * @param[in] input Received motor-command packet.
 * @param[in] length Received byte count.
 * @param[in] command Expected response command.
 * @param[in] minimum_length Minimum safe packet length.
 * @return True when the command envelope is valid.
 */
static bool command_valid(uint8_t previous_sequence, const uint8_t *input, uint16_t length,
                          uint8_t command, uint16_t minimum_length) {
    return input != 0 && length >= minimum_length &&
           length == (((uint16_t)input[1] << 8) | input[2]) + 5 && checksum_valid(input, length) &&
           (input[0] & (MOTOR_COMMAND_PACKET_RESPONSE_FLAG | MOTOR_COMMAND_PACKET_MODE_MASK)) ==
               0 &&
           (input[0] & MOTOR_COMMAND_PACKET_SEQUENCE_MASK) ==
               (previous_sequence & MOTOR_COMMAND_PACKET_SEQUENCE_MASK) &&
           (input[MOTOR_COMMAND_PACKET_FRAGMENT_OFFSET] & MOTOR_COMMAND_PACKET_FRAGMENT_MASK) ==
               0 &&
           input[MOTOR_COMMAND_PACKET_COMMAND_OFFSET] == command;
}

/**
 * @brief Appends a motor-command packet checksum.
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
 * @brief Encodes a motor-command digest request.
 *
 * Builds command 7 with its fixed body length and digest length, normal or retry sequencing, and
 * a big-endian checksum over the first nine bytes.
 *
 * @param[in] sequence Current two-bit sequence, or an out-of-range value to reset it to zero.
 * @param[in] adjacent_sequence Previous sequence for normal mode or next sequence for retry mode.
 * @param[in] retry Selects retry header mode when true.
 * @param[out] output Eleven-byte digest request.
 */
void motor_command_packet_digest_request_encode(
    uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE]) {
    for (uint8_t index = 0; index < MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE; index++) {
        output[index] = 0;
    }
    if (sequence > MOTOR_COMMAND_PACKET_SEQUENCE_MASK) {
        sequence = 0;
    }
    output[0] = (adjacent_sequence & MOTOR_COMMAND_PACKET_SEQUENCE_MASK) |
                (sequence << MOTOR_COMMAND_PACKET_SEQUENCE_SHIFT) |
                ((uint8_t)retry << MOTOR_COMMAND_PACKET_MODE_SHIFT);
    output[2] = MOTOR_COMMAND_PACKET_COMMAND_BODY_LENGTH;
    output[4] = MOTOR_COMMAND_PACKET_DIGEST_COMMAND;
    output[8] = MOTOR_COMMAND_PACKET_DIGEST_LENGTH;
    finish(output, MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE - 2);
}

/**
 * @brief Encodes a motor-command information request.
 *
 * Builds command 5 with the selected information identifier, its fixed response length, normal
 * or retry sequencing, and a big-endian checksum over the first nine bytes.
 *
 * @param[in] selector Information selector from one through nine.
 * @param[in] sequence Current two-bit sequence, or an out-of-range value to reset it to zero.
 * @param[in] adjacent_sequence Previous sequence for normal mode or next sequence for retry mode.
 * @param[in] retry Selects retry header mode when true.
 * @param[out] output Eleven-byte information request.
 * @return True when the selector is supported.
 */
bool motor_command_packet_info_request_encode(
    uint8_t selector, uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE]) {
    if (selector == 0 || selector >= sizeof(info_lengths)) {
        return false;
    }
    for (uint8_t index = 0; index < MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE; index++) {
        output[index] = 0;
    }
    if (sequence > MOTOR_COMMAND_PACKET_SEQUENCE_MASK) {
        sequence = 0;
    }
    output[0] = (adjacent_sequence & MOTOR_COMMAND_PACKET_SEQUENCE_MASK) |
                (sequence << MOTOR_COMMAND_PACKET_SEQUENCE_SHIFT) |
                ((uint8_t)retry << MOTOR_COMMAND_PACKET_MODE_SHIFT);
    output[2] = MOTOR_COMMAND_PACKET_COMMAND_BODY_LENGTH;
    output[4] = MOTOR_COMMAND_PACKET_INFO_COMMAND;
    output[6] = selector;
    output[8] = info_lengths[selector];
    finish(output, MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE - 2);
    return true;
}

/**
 * @brief Encodes a motor-command acknowledgement.
 *
 * Builds a response header carrying the previous sequence and appends a big-endian checksum over
 * its three-byte body.
 *
 * @param[in] previous_sequence Previous two-bit sequence.
 * @param[out] output Five-byte acknowledgement packet.
 */
void motor_command_packet_acknowledgement_encode(
    uint8_t previous_sequence, uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]) {
    output[0] = MOTOR_COMMAND_PACKET_RESPONSE_FLAG |
                (previous_sequence & MOTOR_COMMAND_PACKET_SEQUENCE_MASK);
    output[1] = 0;
    output[2] = 0;
    finish(output, MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 2);
}

/**
 * @brief Encodes a motor-command sequence reset.
 *
 * Builds the response and reset flags with an empty body and appends a big-endian checksum over
 * the first three bytes.
 *
 * @param[out] output Five-byte sequence-reset packet.
 */
void motor_command_packet_sequence_reset_encode(
    uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]) {
    output[0] = MOTOR_COMMAND_PACKET_RESPONSE_FLAG | MOTOR_COMMAND_PACKET_RESET_FLAG;
    output[1] = 0;
    output[2] = 0;
    finish(output, MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 2);
}

/**
 * @brief Encodes a motor-command retry response.
 *
 * Builds the response and retry flags with the next two-bit sequence, an empty body, and a
 * big-endian checksum over the first three bytes.
 *
 * @param[in] next_sequence Next two-bit sequence requested from the peer.
 * @param[out] output Five-byte retry packet.
 */
void motor_command_packet_retry_encode(uint8_t next_sequence,
                                       uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]) {
    output[0] = MOTOR_COMMAND_PACKET_RESPONSE_FLAG | (1U << MOTOR_COMMAND_PACKET_MODE_SHIFT) |
                (next_sequence & MOTOR_COMMAND_PACKET_SEQUENCE_MASK);
    output[1] = 0;
    output[2] = 0;
    finish(output, MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 2);
}

/**
 * @brief Decodes a motor-command digest response.
 *
 * Accepts a checksum-valid, unfragmented command 0x87 carrying the expected prior sequence and
 * copies its sixteen-byte digest source.
 *
 * @param[in] previous_sequence Expected prior two-bit sequence.
 * @param[in] input Received motor-command packet.
 * @param[in] length Received byte count.
 * @param[out] source Sixteen-byte digest source.
 * @return True when the packet contains a valid digest response.
 */
bool motor_command_packet_digest_response_decode(uint8_t previous_sequence, const uint8_t *input,
                                                 uint16_t length,
                                                 uint8_t source[MOTOR_COMMAND_DIGEST_SOURCE_SIZE]) {
    if (source == 0 ||
        !command_valid(previous_sequence, input, length,
                       MOTOR_COMMAND_PACKET_DIGEST_RESPONSE_COMMAND,
                       MOTOR_COMMAND_PACKET_DIGEST_SOURCE_OFFSET +
                           MOTOR_COMMAND_DIGEST_SOURCE_SIZE + MOTOR_COMMAND_PACKET_CHECKSUM_SIZE)) {
        return false;
    }
    for (uint8_t index = 0; index < MOTOR_COMMAND_DIGEST_SOURCE_SIZE; index++) {
        source[index] = input[MOTOR_COMMAND_PACKET_DIGEST_SOURCE_OFFSET + index];
    }
    return true;
}

/**
 * @brief Decodes a motor-command information response.
 *
 * Accepts a checksum-valid, unfragmented command 0x85 for the expected selector and copies its
 * fixed-size payload. Selectors one through nine carry 2, 20, 2, 2, 1, 2, 16, 4, and 50 bytes.
 *
 * @param[in] previous_sequence Expected prior two-bit sequence.
 * @param[in] expected_selector Expected information selector from one through nine.
 * @param[in] input Received motor-command packet.
 * @param[in] length Received byte count.
 * @param[out] output Destination for the selector payload.
 * @param[in] output_capacity Available destination byte count.
 * @return Decoded payload length, or zero when the response is invalid.
 */
uint8_t motor_command_packet_info_response_decode(uint8_t previous_sequence,
                                                  uint8_t expected_selector, const uint8_t *input,
                                                  uint16_t length, uint8_t *output,
                                                  uint8_t output_capacity) {
    if (expected_selector == 0 || expected_selector >= sizeof(info_lengths)) {
        return 0;
    }
    uint8_t data_length = info_lengths[expected_selector];
    uint16_t expected_length =
        MOTOR_COMMAND_PACKET_INFO_SOURCE_OFFSET + data_length + MOTOR_COMMAND_PACKET_CHECKSUM_SIZE;
    if (output == 0 || output_capacity < data_length || length != expected_length ||
        !command_valid(previous_sequence, input, length, MOTOR_COMMAND_PACKET_INFO_RESPONSE_COMMAND,
                       expected_length) ||
        input[MOTOR_COMMAND_PACKET_SELECTOR_OFFSET] != expected_selector ||
        input[MOTOR_COMMAND_PACKET_INFO_SOURCE_OFFSET - 1] != data_length) {
        return 0;
    }
    for (uint8_t index = 0; index < data_length; index++) {
        output[index] = input[MOTOR_COMMAND_PACKET_INFO_SOURCE_OFFSET + index];
    }
    return data_length;
}

/**
 * @brief Decodes a two-byte motor-command information response.
 *
 * Accepts a checksum-valid, unfragmented command 0x85 for selector 3 or 4 and combines its two
 * data bytes in big-endian order.
 *
 * @param[in] previous_sequence Expected prior two-bit sequence.
 * @param[in] expected_selector Expected information selector, 3 or 4.
 * @param[in] input Received motor-command packet.
 * @param[in] length Received byte count.
 * @param[out] value Decoded 16-bit information value.
 * @return True when the packet contains the expected information response.
 */
bool motor_command_packet_info_word_response_decode(uint8_t previous_sequence,
                                                    uint8_t expected_selector, const uint8_t *input,
                                                    uint16_t length, uint16_t *value) {
    uint8_t data[2];
    if (value == 0 || (expected_selector != 3 && expected_selector != 4) ||
        motor_command_packet_info_response_decode(previous_sequence, expected_selector, input,
                                                  length, data, sizeof(data)) != sizeof(data)) {
        return false;
    }
    *value = (uint16_t)((uint16_t)data[0] << 8) | data[1];
    return true;
}
