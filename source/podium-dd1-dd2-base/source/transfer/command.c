#include "transfer/command.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes command-transport ownership and request state.
 *
 * Clears the current owner, completion result, queued request, and response destination.
 *
 * @param[out] transport Command transport to initialize.
 */
void command_transport_init(CommandTransport *transport) { *transport = (CommandTransport){0}; }

/**
 * @brief Claims an unowned command transport.
 *
 * Records the supplied owner only when the transport does not already have an owner.
 *
 * @param[in,out] transport Command transport to claim.
 * @param[in] owner Nonzero owner identifier.
 */
void command_transport_claim(CommandTransport *transport, uint8_t owner) {
    if (transport->owner == 0) {
        transport->owner = owner;
    }
}

/**
 * @brief Releases a command transport held by the supplied owner.
 *
 * Leaves ownership unchanged when the supplied identifier does not match the current owner.
 *
 * @param[in,out] transport Command transport to release.
 * @param[in] owner Owner identifier requesting release.
 */
void command_transport_release(CommandTransport *transport, uint8_t owner) {
    if (transport->owner == owner) {
        transport->owner = 0;
    }
}

/**
 * @brief Tests command-transport ownership.
 *
 * Compares the supplied identifier with the currently registered owner.
 *
 * @param[in] transport Command transport to inspect.
 * @param[in] owner Owner identifier to compare.
 * @return True when the identifiers match.
 */
bool command_transport_is_owner(const CommandTransport *transport, uint8_t owner) {
    return transport->owner == owner;
}

/**
 * @brief Polls and consumes a command-transport completion result.
 *
 * Reports busy while another nonzero owner holds the transport or while a request is active. An
 * idle poll returns the latched result and clears it.
 *
 * @param[in,out] transport Command transport to poll.
 * @param[in] owner Polling owner identifier.
 * @return Busy, complete, or the latched rejection result.
 */
CommandTransportResult command_transport_poll(CommandTransport *transport, uint8_t owner) {
    if ((transport->owner != 0 && transport->owner != owner) ||
        transport->phase != COMMAND_TRANSPORT_IDLE) {
        return COMMAND_TRANSPORT_BUSY;
    }
    CommandTransportResult result = transport->completion;
    transport->completion = COMMAND_TRANSPORT_COMPLETE;
    return result;
}

/**
 * @brief Queues a remote write request.
 *
 * Encodes the owner, write direction, offset, and payload while the transport is idle and has no
 * unconsumed completion result.
 *
 * @param[in,out] transport Command transport receiving the request.
 * @param[in] owner Request owner identifier.
 * @param[in] offset Remote byte offset.
 * @param[in] data Payload bytes, or null when length is zero.
 * @param[in] length Payload byte count.
 * @return Complete when queued, busy when unavailable, or too-long for an invalid payload.
 */
CommandTransportResult command_transport_queue_write(CommandTransport *transport, uint8_t owner,
                                                     uint8_t offset, const uint8_t *data,
                                                     uint16_t length) {
    if (transport->phase != COMMAND_TRANSPORT_IDLE ||
        transport->completion != COMMAND_TRANSPORT_COMPLETE) {
        return COMMAND_TRANSPORT_BUSY;
    }
    uint16_t request_length =
        memory_transfer_encode_write(owner, offset, data, length, transport->request);
    if (request_length == 0) {
        return COMMAND_TRANSPORT_TOO_LONG;
    }
    transport->request_length = request_length;
    transport->phase = COMMAND_TRANSPORT_WRITE_QUEUED;
    return COMMAND_TRANSPORT_COMPLETE;
}

/**
 * @brief Queues a remote read request.
 *
 * Encodes the owner, read direction, offset, and requested length while retaining the destination
 * used by the eventual response.
 *
 * @param[in,out] transport Command transport receiving the request.
 * @param[in] owner Request owner identifier.
 * @param[in] offset Remote byte offset.
 * @param[out] output Destination for returned bytes, or null when length is zero.
 * @param[in] length Requested byte count.
 * @return Complete when queued, busy when unavailable, or too-long for an invalid request.
 */
CommandTransportResult command_transport_queue_read(CommandTransport *transport, uint8_t owner,
                                                    uint8_t offset, uint8_t *output,
                                                    uint16_t length) {
    if (transport->phase != COMMAND_TRANSPORT_IDLE ||
        transport->completion != COMMAND_TRANSPORT_COMPLETE) {
        return COMMAND_TRANSPORT_BUSY;
    }
    uint8_t request_length = memory_transfer_encode_read(owner, offset, length, transport->request);
    if (request_length == 0 || (output == 0 && length != 0)) {
        return COMMAND_TRANSPORT_TOO_LONG;
    }
    transport->request_length = request_length;
    transport->read_output = output;
    transport->read_length = length;
    transport->phase = COMMAND_TRANSPORT_READ_QUEUED;
    return COMMAND_TRANSPORT_COMPLETE;
}

/**
 * @brief Exposes the currently queued command request.
 *
 * Returns the complete group-4 payload only while a read or write is waiting for submission.
 *
 * @param[in] transport Command transport to inspect.
 * @param[out] request Queued request bytes.
 * @param[out] length Queued request byte count.
 * @return True when a request is ready for submission.
 */
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

/**
 * @brief Marks a queued command request as submitted.
 *
 * Selects the corresponding write- or read-response wait state.
 *
 * @param[in,out] transport Command transport whose request was submitted.
 * @return True when a queued request advanced to its response wait state.
 */
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

/**
 * @brief Applies a group-4 response to the active command request.
 *
 * Completes accepted writes, copies accepted read data, and latches direction-specific rejection
 * results. Malformed responses leave the active request pending for a valid response.
 *
 * @param[in,out] transport Command transport awaiting the response.
 * @param[in] response Received group-4 payload.
 * @param[in] length Received payload byte count.
 */
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
