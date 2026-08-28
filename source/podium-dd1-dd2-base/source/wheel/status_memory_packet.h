#ifndef OPENTEC_BASE_WHEEL_STATUS_MEMORY_PACKET_H
#define OPENTEC_BASE_WHEEL_STATUS_MEMORY_PACKET_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE = 5,
    WHEEL_STATUS_MEMORY_DIGEST_REQUEST_SIZE = 11,
};

void wheel_status_memory_digest_request_encode(
    uint8_t sequence, uint8_t adjacent_sequence, bool retry,
    uint8_t output[WHEEL_STATUS_MEMORY_DIGEST_REQUEST_SIZE]);
void wheel_status_memory_acknowledgement_encode(
    uint8_t previous_sequence, uint8_t output[WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE]);
void wheel_status_memory_sequence_reset_encode(
    uint8_t output[WHEEL_STATUS_MEMORY_CONTROL_PACKET_SIZE]);

#endif
