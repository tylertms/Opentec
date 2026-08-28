#include "motor/serial_session.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Initializes a motor serial transport session.
 *
 * Clears both logical message directions, starts the shared sequence at zero, and sets the current
 * packet type to the idle value.
 *
 * @param[out] session Session to initialize.
 */
void motor_serial_session_init(MotorSerialSession *session) {
    if (session == 0) {
        return;
    }
    *session = (MotorSerialSession){0};
    session->control_type = 0xff;
}

/**
 * @brief Queues one logical motor serial message.
 *
 * Copies a message of type two through five into the session and schedules its first data packet.
 *
 * @param[in,out] session Session accepting the message.
 * @param[in] type Message type from two through five.
 * @param[in] message Complete logical message payload.
 * @param[in] length Logical message length from one through 512 bytes.
 * @return True when the idle transmitter accepts the message.
 */
bool motor_serial_session_queue(MotorSerialSession *session, uint8_t type, const uint8_t *message,
                                uint16_t length) {
    if (session == 0 || session->transmit_active || type < MOTOR_SERIAL_MESSAGE_FIRST_TYPE ||
        type > MOTOR_SERIAL_MESSAGE_LAST_TYPE || message == 0 || length == 0 ||
        length > MOTOR_SERIAL_MESSAGE_MAX_SIZE) {
        return false;
    }
    memcpy(session->transmit_message, message, length);
    session->transmit_length = length;
    session->transmit_offset = 0;
    session->transmit_type = type;
    session->control_type = type;
    session->transmit_active = true;
    session->pending_transmit = MOTOR_SERIAL_TRANSMIT_DATA;
    return true;
}

/**
 * @brief Encodes the next packet scheduled by a motor serial session.
 *
 * Emits the current data fragment, a type-one fragment acknowledgement, or a type-zero
 * resynchronization packet, then waits for the peer before scheduling another packet.
 *
 * @param[in,out] session Session with a pending transmission.
 * @param[out] output Encoded sixty-four-byte packet.
 * @return True when a packet was pending and encoded.
 */
bool motor_serial_session_next_packet(MotorSerialSession *session,
                                      uint8_t output[MOTOR_SERIAL_PACKET_SIZE]) {
    if (session == 0 || output == 0 || session->pending_transmit == MOTOR_SERIAL_TRANSMIT_NONE) {
        return false;
    }

    bool encoded = false;
    if (session->pending_transmit == MOTOR_SERIAL_TRANSMIT_DATA && session->transmit_active) {
        encoded = motor_serial_message_fragment_encode(
            session->transmit_type, session->sequence, session->transmit_message,
            session->transmit_length, session->transmit_offset, output, 0, 0);
    } else if (session->pending_transmit == MOTOR_SERIAL_TRANSMIT_ACKNOWLEDGEMENT) {
        encoded = motor_serial_message_acknowledgement_encode(session->sequence, output);
    } else if (session->pending_transmit == MOTOR_SERIAL_TRANSMIT_RESYNCHRONIZATION) {
        encoded = motor_serial_message_resynchronization_encode(session->sequence,
                                                                session->control_type, output);
    }
    if (encoded) {
        session->pending_transmit = MOTOR_SERIAL_TRANSMIT_NONE;
    }
    return encoded;
}

/**
 * @brief Applies one fixed packet to a motor serial session.
 *
 * Valid data and type-one packets advance the shared sequence. Type-one packets advance the
 * outgoing message by 57 bytes. Partial data schedules an acknowledgement, complete data publishes
 * a logical message, and type-zero packets choose resynchronization or data retransmission from the
 * packet sequence.
 *
 * @param[in,out] session Session receiving the packet.
 * @param[in] input Received sixty-four-byte packet.
 * @return Accepted, complete-message, invalid-packet, or overflow result.
 */
MotorSerialSessionResult
motor_serial_session_accept(MotorSerialSession *session,
                            const uint8_t input[MOTOR_SERIAL_PACKET_SIZE]) {
    if (session == 0 || input == 0) {
        return MOTOR_SERIAL_SESSION_INVALID_PACKET;
    }
    if (motor_serial_packet_decode(input, &session->receive_packet) != MOTOR_SERIAL_PACKET_VALID) {
        session->pending_transmit = MOTOR_SERIAL_TRANSMIT_RESYNCHRONIZATION;
        return MOTOR_SERIAL_SESSION_INVALID_PACKET;
    }

    uint8_t type = session->receive_packet.type_flags & MOTOR_SERIAL_PACKET_TYPE_MASK;
    if (type == 0) {
        session->pending_transmit = session->receive_packet.sequence < session->sequence
                                        ? MOTOR_SERIAL_TRANSMIT_RESYNCHRONIZATION
                                    : session->transmit_active ? MOTOR_SERIAL_TRANSMIT_DATA
                                                               : MOTOR_SERIAL_TRANSMIT_NONE;
        return MOTOR_SERIAL_SESSION_ACCEPTED;
    }
    if (type == 1) {
        session->sequence++;
        if (session->transmit_active) {
            uint16_t remaining = session->transmit_length - session->transmit_offset;
            session->transmit_offset += remaining > MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE
                                            ? MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE
                                            : remaining;
            session->pending_transmit = MOTOR_SERIAL_TRANSMIT_DATA;
        }
        return MOTOR_SERIAL_SESSION_ACCEPTED;
    }
    if (type < MOTOR_SERIAL_MESSAGE_FIRST_TYPE || type > MOTOR_SERIAL_MESSAGE_LAST_TYPE) {
        session->pending_transmit = MOTOR_SERIAL_TRANSMIT_RESYNCHRONIZATION;
        return MOTOR_SERIAL_SESSION_INVALID_PACKET;
    }

    session->sequence++;
    MotorSerialMessageResult result =
        motor_serial_message_accept(&session->receive_message, &session->receive_packet);
    if (result == MOTOR_SERIAL_MESSAGE_ACKNOWLEDGE) {
        session->control_type = 1;
        session->pending_transmit = MOTOR_SERIAL_TRANSMIT_ACKNOWLEDGEMENT;
        return MOTOR_SERIAL_SESSION_ACCEPTED;
    }
    if (result == MOTOR_SERIAL_MESSAGE_COMPLETE) {
        session->control_type = 0xff;
        session->receive_complete = true;
        return MOTOR_SERIAL_SESSION_MESSAGE_COMPLETE;
    }
    session->pending_transmit = MOTOR_SERIAL_TRANSMIT_RESYNCHRONIZATION;
    return result == MOTOR_SERIAL_MESSAGE_OVERFLOW ? MOTOR_SERIAL_SESSION_MESSAGE_OVERFLOW
                                                   : MOTOR_SERIAL_SESSION_INVALID_PACKET;
}

/**
 * @brief Exposes the completed incoming motor serial message.
 *
 * Returns the receive assembly after a complete-message result and before it is consumed.
 *
 * @param[in] session Session to inspect.
 * @return Incoming message assembly, or null when no message is complete.
 */
const MotorSerialMessageAssembly *motor_serial_session_message(const MotorSerialSession *session) {
    if (session == 0 || !session->receive_complete) {
        return 0;
    }
    return &session->receive_message;
}

/**
 * @brief Consumes the current incoming motor serial message.
 *
 * Resets the receive assembly for the next logical message.
 *
 * @param[in,out] session Session whose incoming message was handled.
 */
void motor_serial_session_consume_message(MotorSerialSession *session) {
    if (session != 0) {
        motor_serial_message_assembly_reset(&session->receive_message);
        session->receive_complete = false;
    }
}

/**
 * @brief Releases the active outgoing motor serial message.
 *
 * Clears the outgoing payload after its higher-level response or completion has been handled.
 *
 * @param[in,out] session Session whose transmission is complete.
 */
void motor_serial_session_finish_transmit(MotorSerialSession *session) {
    if (session == 0) {
        return;
    }
    session->transmit_length = 0;
    session->transmit_offset = 0;
    session->transmit_type = 0;
    session->transmit_active = false;
    if (session->pending_transmit == MOTOR_SERIAL_TRANSMIT_DATA) {
        session->pending_transmit = MOTOR_SERIAL_TRANSMIT_NONE;
    }
}
