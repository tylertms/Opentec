#include "motor/command_receiver.h"

#include <stdint.h>

#include "motor/command_fragment.h"
#include "motor/command_packet.h"
#include "motor/command_sequence.h"

/**
 * @brief Motor-command packet layout and fragment markers used by the receiver.
 */
enum {
    MOTOR_COMMAND_RECEIVER_HEADER_SIZE = 3,     /**< Number of bytes before the fragment marker. */
    MOTOR_COMMAND_RECEIVER_FRAGMENT_OFFSET = 3, /**< Offset of the fragment marker in a packet. */
    MOTOR_COMMAND_RECEIVER_PAYLOAD_OFFSET = 4,  /**< Offset of the command payload in a packet. */
    MOTOR_COMMAND_RECEIVER_CHECKSUM_SIZE = 2,   /**< Number of trailing checksum bytes. */
    MOTOR_COMMAND_RECEIVER_FRAGMENT_MASK = 7,   /**< Mask for the three-bit fragment marker. */
    MOTOR_COMMAND_RECEIVER_UNFRAGMENTED = 0,    /**< Marker for an unfragmented message. */
    MOTOR_COMMAND_RECEIVER_FIRST_FRAGMENT = 1,  /**< Marker for the first message fragment. */
    MOTOR_COMMAND_RECEIVER_CONTINUATION_FRAGMENT = 2, /**< Marker for a continuation fragment. */
    MOTOR_COMMAND_RECEIVER_FINAL_FRAGMENT = 4,        /**< Marker for the final message fragment. */
    MOTOR_COMMAND_RECEIVER_INVALID_FRAGMENT =
        7, /**< Reserved marker rejected without state change. */
};

void motor_command_receiver_init(MotorCommandReceiver *receiver, uint8_t *assembly,
                                 uint16_t assembly_capacity) {
    motor_command_sequence_init(&receiver->sequence);
    motor_command_fragment_init(&receiver->fragment, assembly, assembly_capacity);
}

MotorCommandReceiveEvent motor_command_receiver_accept(MotorCommandReceiver *receiver,
                                                       const uint8_t *packet, uint16_t length) {
    MotorCommandReceiveEvent event = {.result = MOTOR_COMMAND_RECEIVE_INVALID};
    if (receiver == 0 || packet == 0 || length < MOTOR_COMMAND_RECEIVER_HEADER_SIZE ||
        !motor_command_packet_checksum_valid(packet, length)) {
        return event;
    }
    uint16_t body_length = ((uint16_t)packet[1] << 8) | packet[2];
    if (body_length + MOTOR_COMMAND_RECEIVER_HEADER_SIZE + MOTOR_COMMAND_RECEIVER_CHECKSUM_SIZE !=
        length) {
        return event;
    }
    MotorCommandSequenceEvent sequence_event =
        motor_command_sequence_receive_header(&receiver->sequence, packet[0]);
    if (sequence_event == MOTOR_COMMAND_SEQUENCE_ACKNOWLEDGED) {
        event.result = MOTOR_COMMAND_RECEIVE_ACKNOWLEDGED;
        return event;
    }
    if (sequence_event == MOTOR_COMMAND_SEQUENCE_RESEND) {
        event.result = MOTOR_COMMAND_RECEIVE_RESEND;
        return event;
    }
    if (sequence_event == MOTOR_COMMAND_SEQUENCE_RETRY) {
        event.result = MOTOR_COMMAND_RECEIVE_RETRY;
        return event;
    }
    if (sequence_event == MOTOR_COMMAND_SEQUENCE_RESET) {
        event.result = MOTOR_COMMAND_RECEIVE_RESET;
        return event;
    }
    if (sequence_event != MOTOR_COMMAND_SEQUENCE_PAYLOAD ||
        body_length < MOTOR_COMMAND_RECEIVER_PAYLOAD_OFFSET - MOTOR_COMMAND_RECEIVER_HEADER_SIZE) {
        return event;
    }
    uint8_t fragment_type =
        packet[MOTOR_COMMAND_RECEIVER_FRAGMENT_OFFSET] & MOTOR_COMMAND_RECEIVER_FRAGMENT_MASK;
    if (fragment_type == MOTOR_COMMAND_RECEIVER_UNFRAGMENTED) {
        motor_command_sequence_accept_payload(&receiver->sequence, packet[0]);
        event.result = MOTOR_COMMAND_RECEIVE_MESSAGE;
        event.payload = packet + MOTOR_COMMAND_RECEIVER_PAYLOAD_OFFSET;
        event.payload_length = body_length - 1;
        return event;
    }
    if (fragment_type == MOTOR_COMMAND_RECEIVER_INVALID_FRAGMENT) {
        return event;
    }
    if (fragment_type != MOTOR_COMMAND_RECEIVER_FIRST_FRAGMENT &&
        fragment_type != MOTOR_COMMAND_RECEIVER_CONTINUATION_FRAGMENT &&
        fragment_type != MOTOR_COMMAND_RECEIVER_FINAL_FRAGMENT) {
        motor_command_sequence_accept_payload(&receiver->sequence, packet[0]);
        event.result = MOTOR_COMMAND_RECEIVE_IGNORED;
        return event;
    }
    MotorCommandFragmentResult fragment_result =
        motor_command_fragment_accept(&receiver->fragment, packet, length);
    if (fragment_result == MOTOR_COMMAND_FRAGMENT_INVALID) {
        return event;
    }
    motor_command_sequence_accept_payload(&receiver->sequence, packet[0]);
    if (fragment_result == MOTOR_COMMAND_FRAGMENT_WAITING) {
        event.result = MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING;
        return event;
    }
    event.result = MOTOR_COMMAND_RECEIVE_MESSAGE;
    event.payload = receiver->fragment.data + MOTOR_COMMAND_RECEIVER_PAYLOAD_OFFSET;
    event.payload_length = receiver->fragment.content_length - 1;
    return event;
}
