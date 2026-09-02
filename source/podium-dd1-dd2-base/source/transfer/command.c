#include "transfer/command.h"

#include <stdbool.h>
#include <stdint.h>

void command_transport_init(CommandTransport *transport) { *transport = (CommandTransport){0}; }

void command_transport_claim(CommandTransport *transport, uint8_t owner) {
    if (transport->owner == 0) {
        transport->owner = owner;
    }
}

void command_transport_release(CommandTransport *transport, uint8_t owner) {
    if (transport->owner == owner) {
        transport->owner = 0;
        transport->phase = COMMAND_TRANSPORT_IDLE;
        transport->request_length = 0;
        transport->read_output = 0;
        transport->read_length = 0;
        transport->completion = COMMAND_TRANSPORT_COMPLETE;
    }
}

bool command_transport_is_owner(const CommandTransport *transport, uint8_t owner) {
    return transport->owner == owner;
}

CommandTransportResult command_transport_poll(CommandTransport *transport, uint8_t owner) {
    if ((transport->owner != 0 && transport->owner != owner) ||
        transport->phase != COMMAND_TRANSPORT_IDLE) {
        return COMMAND_TRANSPORT_BUSY;
    }
    CommandTransportResult result = transport->completion;
    transport->completion = COMMAND_TRANSPORT_COMPLETE;
    return result;
}

CommandTransportResult command_transport_queue_write(CommandTransport *transport, uint8_t owner,
                                                     uint8_t offset, const uint8_t *data,
                                                     uint16_t length) {
    return command_transport_queue_write_to(transport, owner, owner, offset, data, length);
}

CommandTransportResult command_transport_queue_write_to(CommandTransport *transport, uint8_t owner,
                                                        uint8_t target, uint8_t offset,
                                                        const uint8_t *data, uint16_t length) {
    if (transport->owner != 0 && !command_transport_is_owner(transport, owner)) {
        return COMMAND_TRANSPORT_BUSY;
    }
    if (transport->phase != COMMAND_TRANSPORT_IDLE ||
        transport->completion != COMMAND_TRANSPORT_COMPLETE) {
        return COMMAND_TRANSPORT_BUSY;
    }
    uint16_t request_length =
        memory_transfer_encode_write(target, offset, data, length, transport->request);
    if (request_length == 0) {
        return COMMAND_TRANSPORT_TOO_LONG;
    }
    transport->request_length = request_length;
    transport->phase = COMMAND_TRANSPORT_WRITE_QUEUED;
    return COMMAND_TRANSPORT_COMPLETE;
}

CommandTransportResult command_transport_queue_read(CommandTransport *transport, uint8_t owner,
                                                    uint8_t offset, uint8_t *output,
                                                    uint16_t length) {
    return command_transport_queue_read_from(transport, owner, owner, offset, output, length);
}

CommandTransportResult command_transport_queue_read_from(CommandTransport *transport, uint8_t owner,
                                                         uint8_t target, uint8_t offset,
                                                         uint8_t *output, uint16_t length) {
    if (transport->owner != 0 && !command_transport_is_owner(transport, owner)) {
        return COMMAND_TRANSPORT_BUSY;
    }
    if (transport->phase != COMMAND_TRANSPORT_IDLE ||
        transport->completion != COMMAND_TRANSPORT_COMPLETE) {
        return COMMAND_TRANSPORT_BUSY;
    }
    uint8_t request_length =
        memory_transfer_encode_read(target, offset, length, transport->request);
    if (request_length == 0 || (output == 0 && length != 0)) {
        return COMMAND_TRANSPORT_TOO_LONG;
    }
    transport->request_length = request_length;
    transport->read_output = output;
    transport->read_length = length;
    transport->phase = COMMAND_TRANSPORT_READ_QUEUED;
    return COMMAND_TRANSPORT_COMPLETE;
}

bool command_transport_request(const CommandTransport *transport, const uint8_t **request,
                               uint16_t *length) {
    if (request == 0 || length == 0 ||
        (transport->phase != COMMAND_TRANSPORT_WRITE_QUEUED &&
         transport->phase != COMMAND_TRANSPORT_READ_QUEUED)) {
        return false;
    }
    *request = transport->request;
    *length = transport->request_length;
    return true;
}

bool command_transport_request_sent(CommandTransport *transport) {
    if (transport->phase == COMMAND_TRANSPORT_WRITE_QUEUED) {
        transport->phase = COMMAND_TRANSPORT_WRITE_PENDING;
        return true;
    }
    if (transport->phase == COMMAND_TRANSPORT_READ_QUEUED) {
        transport->phase = COMMAND_TRANSPORT_READ_PENDING;
        return true;
    }
    return false;
}

void command_transport_fail(CommandTransport *transport) {
    if (transport->phase == COMMAND_TRANSPORT_WRITE_PENDING) {
        transport->completion = COMMAND_TRANSPORT_WRITE_REJECTED;
        transport->phase = COMMAND_TRANSPORT_IDLE;
    } else if (transport->phase == COMMAND_TRANSPORT_READ_PENDING) {
        transport->completion = COMMAND_TRANSPORT_READ_REJECTED;
        transport->phase = COMMAND_TRANSPORT_IDLE;
    }
}

void command_transport_receive(CommandTransport *transport, const uint8_t *response,
                               uint16_t length) {
    MemoryTransferResult result;
    if (transport->phase == COMMAND_TRANSPORT_WRITE_PENDING) {
        result = memory_transfer_decode_write(response, length);
        if (result == MEMORY_TRANSFER_ACCEPTED) {
            transport->phase = COMMAND_TRANSPORT_IDLE;
        } else if (result == MEMORY_TRANSFER_REJECTED) {
            transport->completion = COMMAND_TRANSPORT_WRITE_REJECTED;
            transport->phase = COMMAND_TRANSPORT_IDLE;
        }
        return;
    }
    if (transport->phase == COMMAND_TRANSPORT_READ_PENDING) {
        result = memory_transfer_decode_read(response, length, transport->read_output,
                                             transport->read_length);
        if (result == MEMORY_TRANSFER_ACCEPTED) {
            transport->phase = COMMAND_TRANSPORT_IDLE;
        } else if (result == MEMORY_TRANSFER_REJECTED) {
            transport->completion = COMMAND_TRANSPORT_READ_REJECTED;
            transport->phase = COMMAND_TRANSPORT_IDLE;
        }
        return;
    }
    transport->completion = COMMAND_TRANSPORT_WRITE_REJECTED;
    transport->phase = COMMAND_TRANSPORT_IDLE;
}
