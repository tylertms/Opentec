#ifndef OPENTEC_BASE_TRANSFER_COMMAND_H
#define OPENTEC_BASE_TRANSFER_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/memory.h"

/**
 * @brief Transfer group reserved for command transport requests.
 *
 * Command transport uses this group when submitting encoded remote memory operations.
 */
enum {
    COMMAND_TRANSPORT_GROUP = 4, /**< Group number used by command transport. */
};

/**
 * @brief Result of a command-transport operation or completion poll.
 *
 * Rejection values are bit flags so callers can distinguish read and write failures while the
 * complete, busy, and too-long values represent transport state.
 */
typedef enum {
    COMMAND_TRANSPORT_COMPLETE = 0,       /**< Request queued or no completion error is latched. */
    COMMAND_TRANSPORT_BUSY = 1,           /**< Transport is owned or has another request active. */
    COMMAND_TRANSPORT_WRITE_REJECTED = 2, /**< Remote write request was rejected. */
    COMMAND_TRANSPORT_READ_REJECTED = 4,  /**< Remote read request was rejected. */
    COMMAND_TRANSPORT_TOO_LONG =
        8, /**< Request exceeded a limit or lacked a required destination. */
} CommandTransportResult;

/**
 * @brief Command-transport request phase.
 *
 * The phase identifies whether no request, a queued request, or a submitted request is awaiting a
 * remote response.
 */
typedef enum {
    COMMAND_TRANSPORT_IDLE,         /**< No command request is queued or pending. */
    COMMAND_TRANSPORT_WRITE_QUEUED, /**< A write request is ready for lower transport submission. */
    COMMAND_TRANSPORT_WRITE_PENDING, /**< A submitted write awaits its response. */
    COMMAND_TRANSPORT_READ_QUEUED,   /**< A read request is ready for lower transport submission. */
    COMMAND_TRANSPORT_READ_PENDING,  /**< A submitted read awaits its response. */
} CommandTransportPhase;

/**
 * @brief State for one arbitrated remote memory command transport.
 *
 * The transport stores the encoded request, optional read destination, ownership, phase, and
 * latched completion result shared by command clients.
 */
typedef struct {
    uint8_t request[MEMORY_TRANSFER_MAX_REQUEST_SIZE]; /**< Encoded memory request bytes. */
    uint8_t *read_output;              /**< Destination for an accepted read response. */
    uint16_t request_length;           /**< Number of valid bytes in request. */
    uint16_t read_length;              /**< Number of bytes expected in a read response. */
    CommandTransportResult completion; /**< Latched completion result consumed by polling. */
    CommandTransportPhase phase;       /**< Current request phase. */
    uint8_t owner; /**< Local client currently owning the transport, or zero when unowned. */
} CommandTransport;

/**
 * @brief Initializes command-transport ownership and request state.
 *
 * Clears ownership, queued request data, read-destination state, and completion status.
 *
 * @param[out] transport Command transport to initialize.
 */
void command_transport_init(CommandTransport *transport);

/**
 * @brief Claims an unowned command transport.
 *
 * Records owner only when the transport is currently unowned; an existing owner is retained.
 *
 * @param[in,out] transport Command transport to claim.
 * @param[in] owner Owner identifier requesting the claim.
 */
void command_transport_claim(CommandTransport *transport, uint8_t owner);

/**
 * @brief Releases a command transport held by the supplied owner.
 *
 * Clears ownership only when owner matches the current transport owner.
 *
 * @param[in,out] transport Command transport to release.
 * @param[in] owner Owner identifier requesting release.
 */
void command_transport_release(CommandTransport *transport, uint8_t owner);

/**
 * @brief Tests command-transport ownership.
 *
 * Compares owner with the identifier currently registered by the transport.
 *
 * @param[in] transport Command transport to inspect.
 * @param[in] owner Owner identifier to compare.
 * @return True when owner matches the current transport owner; otherwise false.
 */
bool command_transport_is_owner(const CommandTransport *transport, uint8_t owner);

/**
 * @brief Polls and consumes a command-transport completion result.
 *
 * Returns busy while another owner, request, or completion state prevents polling; otherwise
 * returns and clears the latched completion result.
 *
 * @param[in,out] transport Command transport to poll.
 * @param[in] owner Polling owner identifier.
 * @return Busy, complete, or a latched rejection result.
 */
CommandTransportResult command_transport_poll(CommandTransport *transport, uint8_t owner);

/**
 * @brief Queues a remote write request.
 *
 * Uses owner as both the local arbiter and remote target identifier while the transport is idle.
 *
 * @param[in,out] transport Command transport receiving the request.
 * @param[in] owner Request owner and remote target identifier.
 * @param[in] offset Remote byte offset.
 * @param[in] data Payload bytes, or null when length is zero.
 * @param[in] length Payload byte count.
 * @return Complete when queued, busy when unavailable, or too-long for an invalid payload.
 */
CommandTransportResult command_transport_queue_write(CommandTransport *transport, uint8_t owner,
                                                     uint8_t offset, const uint8_t *data,
                                                     uint16_t length);

/**
 * @brief Queues a remote write for a separately owned client.
 *
 * Uses owner for local arbitration and target in the encoded request so a client can address a
 * separate remote target.
 *
 * @param[in,out] transport Command transport receiving the request.
 * @param[in] owner Local client identifier.
 * @param[in] target Remote target identifier.
 * @param[in] offset Remote byte offset.
 * @param[in] data Payload bytes, or null when length is zero.
 * @param[in] length Payload byte count.
 * @return Complete when queued, busy when unavailable, or too-long for an invalid payload.
 */
CommandTransportResult command_transport_queue_write_to(CommandTransport *transport, uint8_t owner,
                                                        uint8_t target, uint8_t offset,
                                                        const uint8_t *data, uint16_t length);

/**
 * @brief Queues a remote read request.
 *
 * Uses owner as both the local arbiter and remote target identifier and retains output for the
 * accepted response.
 *
 * @param[in,out] transport Command transport receiving the request.
 * @param[in] owner Request owner and remote target identifier.
 * @param[in] offset Remote byte offset.
 * @param[out] output Destination for returned bytes, or null when length is zero.
 * @param[in] length Requested byte count.
 * @return Complete when queued, busy when unavailable, or too-long for an invalid request.
 */
CommandTransportResult command_transport_queue_read(CommandTransport *transport, uint8_t owner,
                                                    uint8_t offset, uint8_t *output,
                                                    uint16_t length);

/**
 * @brief Queues a remote read for a separately owned client.
 *
 * Uses owner for local arbitration and target in the encoded request while retaining output for
 * the accepted response.
 *
 * @param[in,out] transport Command transport receiving the request.
 * @param[in] owner Local client identifier.
 * @param[in] target Remote target identifier.
 * @param[in] offset Remote byte offset.
 * @param[out] output Destination for returned bytes, or null when length is zero.
 * @param[in] length Requested byte count.
 * @return Complete when queued, busy when unavailable, or too-long for an invalid request.
 */
CommandTransportResult command_transport_queue_read_from(CommandTransport *transport, uint8_t owner,
                                                         uint8_t target, uint8_t offset,
                                                         uint8_t *output, uint16_t length);

/**
 * @brief Exposes the currently queued command request.
 *
 * Returns encoded request bytes only while a read or write is queued and waiting for lower
 * transport submission.
 *
 * @param[in] transport Command transport to inspect.
 * @param[out] request Address receiving the queued request buffer address.
 * @param[out] length Destination for the queued request byte count.
 * @return True when a request is ready for submission; otherwise false.
 */
bool command_transport_request(const CommandTransport *transport, const uint8_t **request,
                               uint16_t *length);

/**
 * @brief Marks a queued command request as submitted.
 *
 * Advances a queued read or write to its corresponding response-pending phase.
 *
 * @param[in,out] transport Command transport whose request was submitted.
 * @return True when a queued request advanced; otherwise false.
 */
bool command_transport_request_sent(CommandTransport *transport);

/**
 * @brief Fails the active command request.
 *
 * Latches a direction-specific rejection result and returns a pending request to the idle phase.
 *
 * @param[in,out] transport Command transport whose request failed.
 */
void command_transport_fail(CommandTransport *transport);

/**
 * @brief Applies a remote response to the active command request.
 *
 * Completes accepted writes, copies accepted read data, and latches rejection results; malformed
 * responses leave an active request pending for a valid response, while a transport with no
 * response-pending phase records a write rejection.
 *
 * @param[in,out] transport Command transport awaiting the response.
 * @param[in] response Received group-4 payload.
 * @param[in] length Received payload byte count.
 */
void command_transport_receive(CommandTransport *transport, const uint8_t *response,
                               uint16_t length);

#endif
