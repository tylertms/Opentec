#include "motor/command_fragment.h"

#include <stdint.h>

#include "motor/command_packet.h"

enum {
    MOTOR_COMMAND_FRAGMENT_TYPE_OFFSET = 3,
    MOTOR_COMMAND_FRAGMENT_TYPE_MASK = 7,
    MOTOR_COMMAND_FRAGMENT_FIRST = 1,
    MOTOR_COMMAND_FRAGMENT_CONTINUATION = 2,
    MOTOR_COMMAND_FRAGMENT_FINAL = 4,
    MOTOR_COMMAND_FRAGMENT_CONTINUATION_DATA_OFFSET = 4,
    MOTOR_COMMAND_FRAGMENT_CHECKSUM_SIZE = 2,
    MOTOR_COMMAND_FRAGMENT_ENVELOPE_SIZE = 3,
};

/**
 * @brief Initializes a motor-command fragment assembler.
 *
 * Attaches the caller-owned assembly buffer and clears the assembled and completed content lengths.
 *
 * @param[out] fragment Fragment assembler to initialize.
 * @param[out] data Caller-owned assembly buffer.
 * @param[in] capacity Available assembly buffer byte count.
 */
void motor_command_fragment_init(MotorCommandFragment *fragment, uint8_t *data, uint16_t capacity) {
    fragment->data = data;
    fragment->capacity = capacity;
    fragment->length = 0;
    fragment->content_length = 0;
}

/**
 * @brief Accepts a motor-command packet fragment.
 *
 * A first fragment copies its envelope and body without the checksum. Continuation and final
 * fragments append the declared bytes after their four-byte envelope without the checksum. A
 * final fragment publishes the assembled length after the first three envelope bytes.
 *
 * @param[in,out] fragment Fragment assembler and caller-owned destination.
 * @param[in] packet Candidate fragment packet.
 * @param[in] length Received packet byte count.
 * @return Waiting, complete, or invalid fragment status.
 */
MotorCommandFragmentResult motor_command_fragment_accept(MotorCommandFragment *fragment,
                                                         const uint8_t *packet, uint16_t length) {
    if (fragment == 0 || fragment->data == 0 || packet == 0 ||
        !motor_command_packet_checksum_valid(packet, length)) {
        return MOTOR_COMMAND_FRAGMENT_INVALID;
    }
    uint16_t body_length = ((uint16_t)packet[1] << 8) | packet[2];
    uint16_t body_end = body_length + MOTOR_COMMAND_FRAGMENT_ENVELOPE_SIZE;
    if (body_end < MOTOR_COMMAND_FRAGMENT_CONTINUATION_DATA_OFFSET ||
        body_end > length - MOTOR_COMMAND_FRAGMENT_CHECKSUM_SIZE) {
        return MOTOR_COMMAND_FRAGMENT_INVALID;
    }
    uint8_t type = packet[MOTOR_COMMAND_FRAGMENT_TYPE_OFFSET] & MOTOR_COMMAND_FRAGMENT_TYPE_MASK;
    if (type == MOTOR_COMMAND_FRAGMENT_FIRST) {
        if (body_end > fragment->capacity) {
            return MOTOR_COMMAND_FRAGMENT_INVALID;
        }
        for (uint16_t index = 0; index < body_end; index++) {
            fragment->data[index] = packet[index];
        }
        fragment->length = body_end;
        fragment->content_length = 0;
        return MOTOR_COMMAND_FRAGMENT_WAITING;
    }
    if ((type != MOTOR_COMMAND_FRAGMENT_CONTINUATION && type != MOTOR_COMMAND_FRAGMENT_FINAL) ||
        fragment->length == 0) {
        return MOTOR_COMMAND_FRAGMENT_INVALID;
    }
    uint16_t append_length = body_end - MOTOR_COMMAND_FRAGMENT_CONTINUATION_DATA_OFFSET;
    if (fragment->length > fragment->capacity ||
        append_length > fragment->capacity - fragment->length) {
        return MOTOR_COMMAND_FRAGMENT_INVALID;
    }
    for (uint16_t index = 0; index < append_length; index++) {
        fragment->data[fragment->length + index] =
            packet[MOTOR_COMMAND_FRAGMENT_CONTINUATION_DATA_OFFSET + index];
    }
    fragment->length += append_length;
    if (type == MOTOR_COMMAND_FRAGMENT_FINAL) {
        fragment->content_length = fragment->length - MOTOR_COMMAND_FRAGMENT_ENVELOPE_SIZE;
        return MOTOR_COMMAND_FRAGMENT_COMPLETE;
    }
    return MOTOR_COMMAND_FRAGMENT_WAITING;
}
