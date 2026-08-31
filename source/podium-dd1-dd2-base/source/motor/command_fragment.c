#include "motor/command_fragment.h"

#include <stdint.h>

#include "motor/command_packet.h"

/** @brief Internal packet offsets and fragment markers used by the assembler. */
enum {
    MOTOR_COMMAND_FRAGMENT_TYPE_OFFSET = 3, /**< Packet offset of the fragment marker. */
    MOTOR_COMMAND_FRAGMENT_TYPE_MASK = 7, /**< Mask selecting fragment marker bits. */
    MOTOR_COMMAND_FRAGMENT_FIRST = 1, /**< Marker for the first fragment. */
    MOTOR_COMMAND_FRAGMENT_CONTINUATION = 2, /**< Marker for a continuation fragment. */
    MOTOR_COMMAND_FRAGMENT_FINAL = 4, /**< Marker for the final fragment. */
    MOTOR_COMMAND_FRAGMENT_CONTINUATION_DATA_OFFSET = 4, /**< Packet offset of continuation data. */
    MOTOR_COMMAND_FRAGMENT_CHECKSUM_SIZE = 2, /**< Number of checksum bytes excluded from fragment data. */
    MOTOR_COMMAND_FRAGMENT_ENVELOPE_SIZE = 3, /**< Number of packet envelope bytes before the body. */
};

void motor_command_fragment_init(MotorCommandFragment *fragment, uint8_t *data, uint16_t capacity) {
    fragment->data = data;
    fragment->capacity = capacity;
    fragment->length = 0;
    fragment->content_length = 0;
}

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
