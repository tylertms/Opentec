#include "motor/command_receiver.h"

#include <stdint.h>

#include "motor/command_fragment.h"
#include "motor/command_packet.h"
#include "motor/command_sequence.h"

enum {
    MOTOR_COMMAND_RECEIVER_HEADER_SIZE = 3,
    MOTOR_COMMAND_RECEIVER_FRAGMENT_OFFSET = 3,
    MOTOR_COMMAND_RECEIVER_PAYLOAD_OFFSET = 4,
    MOTOR_COMMAND_RECEIVER_CHECKSUM_SIZE = 2,
    MOTOR_COMMAND_RECEIVER_FRAGMENT_MASK = 7,
    MOTOR_COMMAND_RECEIVER_UNFRAGMENTED = 0,
    MOTOR_COMMAND_RECEIVER_FIRST_FRAGMENT = 1,
    MOTOR_COMMAND_RECEIVER_CONTINUATION_FRAGMENT = 2,
    MOTOR_COMMAND_RECEIVER_FINAL_FRAGMENT = 4,
    MOTOR_COMMAND_RECEIVER_INVALID_FRAGMENT = 7,
};

/**
 * @brief Initializes a motor-command receiver session.
 *
 * Resets transmitted and received sequence tracking and attaches the caller-owned fragment
 * assembly buffer.
 *
 * @param[out] receiver Receiver session to initialize.
 * @param[out] assembly Caller-owned fragment assembly buffer.
 * @param[in] assembly_capacity Available assembly buffer byte count.
 */
void motor_command_receiver_init(MotorCommandReceiver *receiver, uint8_t *assembly,
                                 uint16_t assembly_capacity) {
    motor_command_sequence_init(&receiver->sequence);
    motor_command_fragment_init(&receiver->fragment, assembly, assembly_capacity);
}

/**
 * @brief Accepts one motor-command packet.
 *
 * Validates the checksum and declared packet length, applies acknowledgement, retry, reset, and
 * resend sequence state, and assembles first, continuation, and final payload fragments. Complete
 * messages expose only their command payload after the fragment marker.
 *
 * @param[in,out] receiver Receiver session to advance.
 * @param[in] packet Received motor-command packet.
 * @param[in] length Received packet byte count.
 * @return Sequence event, fragment progress, complete message, ignored fragment, or invalid input.
 */
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
