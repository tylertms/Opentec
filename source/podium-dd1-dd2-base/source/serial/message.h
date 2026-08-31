#ifndef OPENTEC_BASE_SERIAL_MESSAGE_H
#define OPENTEC_BASE_SERIAL_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/packet.h"

enum {
    SERIAL_MESSAGE_MAX_SIZE = 515,
    SERIAL_MESSAGE_FIRST_TYPE = 2,
    SERIAL_MESSAGE_LAST_TYPE = 5,
};

typedef enum {
    SERIAL_MESSAGE_INVALID_PACKET,
    SERIAL_MESSAGE_ACKNOWLEDGE,
    SERIAL_MESSAGE_COMPLETE,
    SERIAL_MESSAGE_OVERFLOW,
} SerialMessageResult;

typedef struct {
    uint8_t data[SERIAL_MESSAGE_MAX_SIZE];
    uint16_t length;
    uint8_t type;
} SerialMessageAssembly;

void serial_message_assembly_reset(SerialMessageAssembly *assembly);
bool serial_message_fragment_encode(uint8_t type, uint8_t sequence, const uint8_t *message,
                                    uint16_t message_length, uint16_t offset,
                                    uint8_t output[SERIAL_PACKET_SIZE], uint16_t *next_offset,
                                    bool *acknowledgement_required);
SerialMessageResult serial_message_accept(SerialMessageAssembly *assembly,
                                          const SerialPacket *packet);
bool serial_message_acknowledgement_encode(uint8_t sequence, uint8_t current_type,
                                           uint8_t output[SERIAL_PACKET_SIZE]);
bool serial_message_resynchronization_encode(uint8_t sequence, uint8_t current_type,
                                             uint8_t output[SERIAL_PACKET_SIZE]);

#endif
