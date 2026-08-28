#ifndef OPENTEC_BASE_TRANSFER_COMMAND_H
#define OPENTEC_BASE_TRANSFER_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/memory.h"

enum {
    COMMAND_TRANSPORT_GROUP = 4,
};

typedef enum {
    COMMAND_TRANSPORT_COMPLETE = 0,
    COMMAND_TRANSPORT_BUSY = 1,
    COMMAND_TRANSPORT_WRITE_REJECTED = 2,
    COMMAND_TRANSPORT_READ_REJECTED = 4,
    COMMAND_TRANSPORT_TOO_LONG = 8,
} CommandTransportResult;

typedef enum {
    COMMAND_TRANSPORT_IDLE,
    COMMAND_TRANSPORT_WRITE_QUEUED,
    COMMAND_TRANSPORT_WRITE_PENDING,
    COMMAND_TRANSPORT_READ_QUEUED,
    COMMAND_TRANSPORT_READ_PENDING,
} CommandTransportPhase;

typedef struct {
    uint8_t request[MEMORY_TRANSFER_MAX_REQUEST_SIZE];
    uint8_t *read_output;
    uint16_t request_length;
    uint16_t read_length;
    CommandTransportResult completion;
    CommandTransportPhase phase;
    uint8_t owner;
} CommandTransport;

void command_transport_init(CommandTransport *transport);
void command_transport_claim(CommandTransport *transport, uint8_t owner);
void command_transport_release(CommandTransport *transport, uint8_t owner);
bool command_transport_is_owner(const CommandTransport *transport, uint8_t owner);
CommandTransportResult command_transport_poll(CommandTransport *transport, uint8_t owner);
CommandTransportResult command_transport_queue_write(CommandTransport *transport, uint8_t owner,
                                                     uint8_t offset, const uint8_t *data,
                                                     uint16_t length);
CommandTransportResult command_transport_queue_read(CommandTransport *transport, uint8_t owner,
                                                    uint8_t offset, uint8_t *output,
                                                    uint16_t length);
bool command_transport_request(const CommandTransport *transport, const uint8_t **request,
                               uint16_t *length);
bool command_transport_request_sent(CommandTransport *transport);
void command_transport_receive(CommandTransport *transport, const uint8_t *response,
                               uint16_t length);

#endif
