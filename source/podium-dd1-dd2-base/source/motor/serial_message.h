#ifndef OPENTEC_BASE_MOTOR_SERIAL_MESSAGE_H
#define OPENTEC_BASE_MOTOR_SERIAL_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/serial_packet.h"

enum {
    MOTOR_SERIAL_MESSAGE_MAX_SIZE = 512,
    MOTOR_SERIAL_MESSAGE_FIRST_TYPE = 2,
    MOTOR_SERIAL_MESSAGE_LAST_TYPE = 5,
};

typedef enum {
    MOTOR_SERIAL_MESSAGE_INVALID_PACKET,
    MOTOR_SERIAL_MESSAGE_ACKNOWLEDGE,
    MOTOR_SERIAL_MESSAGE_COMPLETE,
    MOTOR_SERIAL_MESSAGE_OVERFLOW,
} MotorSerialMessageResult;

typedef struct {
    uint8_t data[MOTOR_SERIAL_MESSAGE_MAX_SIZE];
    uint16_t length;
    uint8_t type;
} MotorSerialMessageAssembly;

void motor_serial_message_assembly_reset(MotorSerialMessageAssembly *assembly);
bool motor_serial_message_fragment_encode(uint8_t type, uint8_t sequence, const uint8_t *message,
                                          uint16_t message_length, uint16_t offset,
                                          uint8_t output[MOTOR_SERIAL_PACKET_SIZE],
                                          uint16_t *next_offset, bool *acknowledgement_required);
MotorSerialMessageResult motor_serial_message_accept(MotorSerialMessageAssembly *assembly,
                                                     const MotorSerialPacket *packet);
bool motor_serial_message_acknowledgement_encode(uint8_t sequence,
                                                 uint8_t output[MOTOR_SERIAL_PACKET_SIZE]);
bool motor_serial_message_resynchronization_encode(uint8_t sequence, uint8_t current_type,
                                                   uint8_t output[MOTOR_SERIAL_PACKET_SIZE]);

#endif
