#ifndef OPENTEC_BASE_MOTOR_COMMAND_PACKET_H
#define OPENTEC_BASE_MOTOR_COMMAND_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_digest.h"

enum {
    MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE = 5,
    MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE = 11,
    MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE = 11,
    MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE = 1009,
};

void motor_command_packet_digest_request_encode(
    uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[MOTOR_COMMAND_PACKET_DIGEST_REQUEST_SIZE]);
bool motor_command_packet_info_request_encode(
    uint8_t selector, uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[MOTOR_COMMAND_PACKET_INFO_REQUEST_SIZE]);
void motor_command_packet_acknowledgement_encode(
    uint8_t previous_sequence, uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]);
void motor_command_packet_sequence_reset_encode(
    uint8_t output[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE]);
bool motor_command_packet_digest_response_decode(uint8_t previous_sequence, const uint8_t *input,
                                                 uint16_t length,
                                                 uint8_t source[MOTOR_COMMAND_DIGEST_SOURCE_SIZE]);
bool motor_command_packet_info_word_response_decode(uint8_t previous_sequence,
                                                    uint8_t expected_selector, const uint8_t *input,
                                                    uint16_t length, uint16_t *value);

#endif
