#ifndef OPENTEC_BASE_SERIAL_SESSION_H
#define OPENTEC_BASE_SERIAL_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/message.h"
#include "serial/packet.h"

typedef enum {
    SERIAL_SESSION_ACCEPTED,
    SERIAL_SESSION_MESSAGE_COMPLETE,
    SERIAL_SESSION_INVALID_PACKET,
    SERIAL_SESSION_MESSAGE_OVERFLOW,
} SerialSessionResult;

typedef enum {
    SERIAL_TRANSMIT_NONE,
    SERIAL_TRANSMIT_DATA,
    SERIAL_TRANSMIT_ACKNOWLEDGEMENT,
    SERIAL_TRANSMIT_RESYNCHRONIZATION,
} SerialTransmitKind;

typedef struct {
    uint8_t transmit_message[SERIAL_MESSAGE_MAX_SIZE];
    SerialMessageAssembly receive_message;
    SerialPacket receive_packet;
    uint16_t transmit_length;
    uint16_t transmit_offset;
    SerialTransmitKind pending_transmit;
    uint8_t transmit_type;
    uint8_t control_type;
    uint8_t sequence;
    bool receive_complete;
    bool transmit_active;
} SerialSession;

void serial_session_init(SerialSession *session);
bool serial_session_queue(SerialSession *session, uint8_t type, const uint8_t *message,
                          uint16_t length);
bool serial_session_next_packet(SerialSession *session, uint8_t output[SERIAL_PACKET_SIZE]);
SerialSessionResult serial_session_accept(SerialSession *session,
                                          const uint8_t input[SERIAL_PACKET_SIZE]);
const SerialMessageAssembly *serial_session_message(const SerialSession *session);
void serial_session_consume_message(SerialSession *session);
void serial_session_finish_transmit(SerialSession *session);

#endif
