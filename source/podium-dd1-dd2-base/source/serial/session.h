#ifndef OPENTEC_BASE_SERIAL_SESSION_H
#define OPENTEC_BASE_SERIAL_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/message.h"
#include "serial/packet.h"

/** @brief Result of applying one packet to a serial session. */
typedef enum {
    SERIAL_SESSION_ACCEPTED,         /**< Packet was accepted and no logical message completed. */
    SERIAL_SESSION_MESSAGE_COMPLETE, /**< Packet completed an incoming logical message. */
    SERIAL_SESSION_INVALID_PACKET,   /**< Packet was invalid or had an unsupported type. */
    SERIAL_SESSION_MESSAGE_OVERFLOW, /**< Packet would exceed the incoming message capacity. */
} SerialSessionResult;

/** @brief Kind of packet currently scheduled for transmission. */
typedef enum {
    SERIAL_TRANSMIT_NONE,              /**< No packet is scheduled. */
    SERIAL_TRANSMIT_DATA,              /**< A logical-message data fragment is scheduled. */
    SERIAL_TRANSMIT_ACKNOWLEDGEMENT,   /**< A fragment acknowledgement is scheduled. */
    SERIAL_TRANSMIT_RESYNCHRONIZATION, /**< A resynchronization packet is scheduled. */
} SerialTransmitKind;

/** @brief Shared state for serial message transmission and reception. */
typedef struct {
    uint8_t transmit_message[SERIAL_MESSAGE_MAX_SIZE]; /**< Outgoing logical-message storage. */
    SerialMessageAssembly receive_message;             /**< Incoming logical-message assembly. */
    SerialPacket receive_packet;         /**< Storage for the most recently decoded packet. */
    uint16_t transmit_length;            /**< Number of valid bytes in transmit_message. */
    uint16_t transmit_offset;            /**< Offset of the next outgoing fragment. */
    SerialTransmitKind pending_transmit; /**< Packet kind awaiting encoding. */
    uint8_t transmit_type;               /**< Logical type of the outgoing message. */
    uint8_t control_type;                /**< Logical type carried by control packets. */
    uint8_t sequence;                    /**< Shared transport sequence number. */
    bool receive_complete; /**< Whether receive_message contains a complete message. */
    bool transmit_active;  /**< Whether an outgoing logical message is active. */
} SerialSession;

/**
 * @brief Initializes a serial session.
 *
 * Clears incoming and outgoing state and initializes the control type to its idle value.
 *
 * @param[out] session Session state to initialize.
 */
void serial_session_init(SerialSession *session);

/**
 * @brief Queues one outgoing logical message.
 *
 * Copies a valid message into session-owned storage and schedules its first data fragment.
 *
 * @param[in,out] session Idle session accepting the message.
 * @param[in] type Logical message type from SERIAL_MESSAGE_FIRST_TYPE through
 * SERIAL_MESSAGE_LAST_TYPE.
 * @param[in] message Complete logical message bytes.
 * @param[in] length Logical message length from one through SERIAL_MESSAGE_MAX_SIZE bytes.
 * @return True when the message is accepted; otherwise false.
 */
bool serial_session_queue(SerialSession *session, uint8_t type, const uint8_t *message,
                          uint16_t length);

/**
 * @brief Encodes the next scheduled session packet.
 *
 * Encodes a data, acknowledgement, or resynchronization packet and clears its pending state when
 * encoding succeeds.
 *
 * @param[in,out] session Session with a scheduled packet.
 * @param[out] output Destination for the encoded packet.
 * @return True when a pending packet is encoded; otherwise false.
 */
bool serial_session_next_packet(SerialSession *session, uint8_t output[SERIAL_PACKET_SIZE]);

/**
 * @brief Accepts one encoded packet in a serial session.
 *
 * Decodes the packet, advances the shared sequence for accepted acknowledgement and data packets,
 * and schedules any required response.
 *
 * @param[in,out] session Session state to update.
 * @param[in] input Received encoded packet.
 * @return Accepted, complete, invalid, or overflow result.
 */
SerialSessionResult serial_session_accept(SerialSession *session,
                                          const uint8_t input[SERIAL_PACKET_SIZE]);

/**
 * @brief Returns a completed incoming message.
 *
 * Provides the receive assembly until it is consumed or the session is reset.
 *
 * @param[in] session Session state to inspect.
 * @return Completed message assembly, or null when no message is complete.
 */
const SerialMessageAssembly *serial_session_message(const SerialSession *session);

/**
 * @brief Consumes the completed incoming message.
 *
 * Resets the receive assembly and clears its completion flag.
 *
 * @param[in,out] session Session state whose message was handled.
 */
void serial_session_consume_message(SerialSession *session);

/**
 * @brief Finishes the active outgoing message.
 *
 * Clears outgoing payload state and removes a pending data packet while preserving sequence state.
 *
 * @param[in,out] session Session state whose transmission is finished.
 */
void serial_session_finish_transmit(SerialSession *session);

#endif
