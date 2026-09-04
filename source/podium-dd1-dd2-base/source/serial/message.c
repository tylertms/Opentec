#include "serial/message.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Resets an attached-device serial message assembly.
 *
 * Discards the accumulated length and type so the assembly can accept a new message.
 *
 * @param[out] assembly Message assembly to reset.
 */
void serial_message_assembly_reset(SerialMessageAssembly *assembly) {
    if (assembly == 0) {
        return;
    }
    assembly->length = 0;
    assembly->type = 0;
}

/**
 * @brief Encodes one fragment of an attached-device serial message.
 *
 * Emits an unmarked packet for messages through fifty-seven bytes. Longer messages use a
 * fifty-seven-byte first fragment, zero or more fifty-seven-byte continuation fragments, and a
 * final fragment containing the remainder. The logical message can contain up to
 * SERIAL_MESSAGE_MAX_SIZE bytes.
 *
 * @param[in] type Message type from two through five.
 * @param[in] sequence Transport sequence byte.
 * @param[in] message Complete logical message payload.
 * @param[in] message_length Logical message length from one through SERIAL_MESSAGE_MAX_SIZE bytes.
 * @param[in] offset Byte offset of the fragment to encode.
 * @param[out] output Encoded sixty-four-byte packet.
 * @param[out] next_offset Offset immediately after the encoded fragment, or null when unused.
 * @param[out] acknowledgement_required True for first and continuation fragments, or null when
 * unused.
 * @return True when the message, offset, and output arguments are usable.
 */
bool serial_message_fragment_encode(uint8_t type, uint8_t sequence, const uint8_t *message,
                                    uint16_t message_length, uint16_t offset,
                                    uint8_t output[SERIAL_PACKET_SIZE], uint16_t *next_offset,
                                    bool *acknowledgement_required) {
    if (type < SERIAL_MESSAGE_FIRST_TYPE || type > SERIAL_MESSAGE_LAST_TYPE || message == 0 ||
        message_length == 0 || message_length > SERIAL_MESSAGE_MAX_SIZE ||
        offset >= message_length) {
        return false;
    }

    uint16_t remaining = message_length - offset;
    uint8_t payload_length = remaining > SERIAL_PACKET_MAX_PAYLOAD_SIZE
                                 ? SERIAL_PACKET_MAX_PAYLOAD_SIZE
                                 : (uint8_t)remaining;
    uint8_t fragment = 0;
    if (message_length > SERIAL_PACKET_MAX_PAYLOAD_SIZE) {
        if (offset == 0) {
            fragment = SERIAL_PACKET_FIRST_FRAGMENT;
        } else if (remaining > SERIAL_PACKET_MAX_PAYLOAD_SIZE) {
            fragment = SERIAL_PACKET_CONTINUATION_FRAGMENT;
        } else {
            fragment = SERIAL_PACKET_FINAL_FRAGMENT;
        }
    }

    if (!serial_packet_encode(type | fragment, sequence, message + offset, payload_length,
                              output)) {
        return false;
    }
    if (next_offset != 0) {
        *next_offset = offset + payload_length;
    }
    if (acknowledgement_required != 0) {
        *acknowledgement_required = fragment == SERIAL_PACKET_FIRST_FRAGMENT ||
                                    fragment == SERIAL_PACKET_CONTINUATION_FRAGMENT;
    }
    return true;
}

/**
 * @brief Accumulates one decoded attached-device serial message packet.
 *
 * Appends packets of types two through five to the current SERIAL_MESSAGE_MAX_SIZE-byte message.
 * First and continuation fragments request an acknowledgement; an unmarked or final fragment
 * completes the message. An over-capacity packet clears the entire assembly before returning
 * SERIAL_MESSAGE_OVERFLOW.
 *
 * @param[in,out] assembly Message payload and type accumulated across packets.
 * @param[in] packet Decoded attached-device serial packet.
 * @return Invalid-packet, acknowledgement, complete-message, or overflow result.
 */
SerialMessageResult serial_message_accept(SerialMessageAssembly *assembly,
                                          const SerialPacket *packet) {
    if (assembly == 0 || packet == 0) {
        return SERIAL_MESSAGE_INVALID_PACKET;
    }
    uint8_t type = packet->type_flags & SERIAL_PACKET_TYPE_MASK;
    if (type < SERIAL_MESSAGE_FIRST_TYPE || type > SERIAL_MESSAGE_LAST_TYPE ||
        (assembly->length != 0 && assembly->type != type)) {
        return SERIAL_MESSAGE_INVALID_PACKET;
    }
    if (assembly->length > SERIAL_MESSAGE_MAX_SIZE ||
        packet->payload_length > SERIAL_MESSAGE_MAX_SIZE - assembly->length) {
        serial_message_assembly_reset(assembly);
        return SERIAL_MESSAGE_OVERFLOW;
    }
    if (assembly->length == 0) {
        assembly->type = type;
    }
    if (packet->payload_length != 0) {
        memcpy(assembly->data + assembly->length, packet->payload, packet->payload_length);
        assembly->length += packet->payload_length;
    }
    return (packet->type_flags &
            (SERIAL_PACKET_FIRST_FRAGMENT | SERIAL_PACKET_CONTINUATION_FRAGMENT)) != 0
               ? SERIAL_MESSAGE_ACKNOWLEDGE
               : SERIAL_MESSAGE_COMPLETE;
}

/**
 * @brief Encodes an attached-device serial fragment acknowledgement.
 *
 * Builds a type-one packet whose one-byte payload identifies the acknowledgement control type.
 *
 * @param[in] sequence Transport sequence byte.
 * @param[in] current_type Acknowledged transport control type.
 * @param[out] output Encoded sixty-four-byte packet.
 * @return True when the destination is usable.
 */
bool serial_message_acknowledgement_encode(uint8_t sequence, uint8_t current_type,
                                           uint8_t output[SERIAL_PACKET_SIZE]) {
    return serial_packet_encode_byte(1, sequence, current_type, output);
}

/**
 * @brief Encodes an attached-device serial resynchronization packet.
 *
 * Builds a type-zero packet whose one-byte payload identifies the current transport packet type.
 *
 * @param[in] sequence Transport sequence byte.
 * @param[in] current_type Current transport packet type.
 * @param[out] output Encoded sixty-four-byte packet.
 * @return True when the destination is usable.
 */
bool serial_message_resynchronization_encode(uint8_t sequence, uint8_t current_type,
                                             uint8_t output[SERIAL_PACKET_SIZE]) {
    return serial_packet_encode_byte(0, sequence, current_type, output);
}
