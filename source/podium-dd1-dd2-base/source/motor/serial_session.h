#ifndef OPENTEC_BASE_MOTOR_SERIAL_SESSION_H
#define OPENTEC_BASE_MOTOR_SERIAL_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/serial_message.h"
#include "motor/serial_packet.h"

typedef enum {
    MOTOR_SERIAL_SESSION_ACCEPTED,
    MOTOR_SERIAL_SESSION_MESSAGE_COMPLETE,
    MOTOR_SERIAL_SESSION_INVALID_PACKET,
    MOTOR_SERIAL_SESSION_MESSAGE_OVERFLOW,
} MotorSerialSessionResult;

typedef enum {
    MOTOR_SERIAL_TRANSMIT_NONE,
    MOTOR_SERIAL_TRANSMIT_DATA,
    MOTOR_SERIAL_TRANSMIT_ACKNOWLEDGEMENT,
    MOTOR_SERIAL_TRANSMIT_RESYNCHRONIZATION,
} MotorSerialTransmitKind;

typedef struct {
    uint8_t transmit_message[MOTOR_SERIAL_MESSAGE_MAX_SIZE];
    MotorSerialMessageAssembly receive_message;
    MotorSerialPacket receive_packet;
    uint16_t transmit_length;
    uint16_t transmit_offset;
    MotorSerialTransmitKind pending_transmit;
    uint8_t transmit_type;
    uint8_t control_type;
    uint8_t sequence;
    bool receive_complete;
    bool transmit_active;
} MotorSerialSession;

void motor_serial_session_init(MotorSerialSession *session);
bool motor_serial_session_queue(MotorSerialSession *session, uint8_t type, const uint8_t *message,
                                uint16_t length);
bool motor_serial_session_next_packet(MotorSerialSession *session,
                                      uint8_t output[MOTOR_SERIAL_PACKET_SIZE]);
MotorSerialSessionResult motor_serial_session_accept(MotorSerialSession *session,
                                                     const uint8_t input[MOTOR_SERIAL_PACKET_SIZE]);
const MotorSerialMessageAssembly *motor_serial_session_message(const MotorSerialSession *session);
void motor_serial_session_consume_message(MotorSerialSession *session);
void motor_serial_session_finish_transmit(MotorSerialSession *session);

#endif
