#ifndef OPENTEC_BASE_WHEEL_COMMAND_FORWARDER_H
#define OPENTEC_BASE_WHEEL_COMMAND_FORWARDER_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/**
 * @brief Maximum serialized payload retained by the generic command forwarder.
 *
 * The size matches the largest attached-device command record batch accepted by the service.
 */
enum {
    WHEEL_COMMAND_FORWARDER_PAYLOAD_SIZE = 61, /**< Maximum retained command-payload bytes. */
};

/**
 * @brief Phase of generic attached-device command forwarding.
 *
 * The phase tracks endpoint discovery and the pending or ready state of a forwarded write.
 */
typedef enum {
    WHEEL_COMMAND_FORWARDER_PROBE_READY,   /**< A probe may be queued for the selected endpoint. */
    WHEEL_COMMAND_FORWARDER_PROBE_PENDING, /**< An endpoint probe is in flight. */
    WHEEL_COMMAND_FORWARDER_READY, /**< The selected endpoint is ready for a forwarded write. */
    WHEEL_COMMAND_FORWARDER_WRITE_PENDING, /**< A forwarded write is in flight. */
} WheelCommandForwarderPhase;

/**
 * @brief Generic attached-device command forwarding state.
 *
 * The state retains one serialized batch while discovering and writing through the selected
 * standard or extended endpoint.
 */
typedef struct {
    uint8_t
        payload[WHEEL_COMMAND_FORWARDER_PAYLOAD_SIZE]; /**< Retained serialized command batch. */
    uint8_t probe[4];                                  /**< Endpoint probe response bytes. */
    uint8_t payload_length; /**< Number of valid bytes retained in payload. */
    uint8_t endpoint_index; /**< Selected endpoint index, zero for standard or one for extended. */
    WheelCommandForwarderPhase phase; /**< Current discovery or write phase. */
    uint16_t wait_calls;              /**< Consecutive busy polls for the active probe or write. */
} WheelCommandForwarder;

/**
 * @brief Initializes generic attached-device command forwarding.
 *
 * Clears retained payload and probe state and selects the standard endpoint for the first probe.
 *
 * @param[out] forwarder Command forwarder to initialize.
 */
void wheel_command_forwarder_init(WheelCommandForwarder *forwarder);

/**
 * @brief Reports whether another generic command batch can be retained.
 *
 * A batch can be accepted only when the forwarder exists and its retained payload area is empty.
 *
 * @param[in] forwarder Command forwarder to inspect.
 * @return true when a new batch can be retained; false when forwarder is null or a batch is queued.
 */
bool wheel_command_forwarder_accepting(const WheelCommandForwarder *forwarder);

/**
 * @brief Queues one generic attached-device command batch.
 *
 * Copies a nonempty batch into retained storage when its length does not exceed the forwarder's
 * payload capacity and no other batch is waiting.
 *
 * @param[in,out] forwarder Command forwarder accepting the batch.
 * @param[in] payload Serialized complete command records to copy.
 * @param[in] length Number of bytes in payload.
 * @return true when the batch was copied; false for null pointers, zero length, excess length, or
 * an already queued batch.
 */
bool wheel_command_forwarder_queue(WheelCommandForwarder *forwarder, const uint8_t *payload,
                                   uint8_t length);

/**
 * @brief Advances generic attached-device command forwarding.
 *
 * Discovers an endpoint when work arrives, writes a retained batch when the endpoint is ready, and
 * restarts discovery after a rejected or timed-out transfer. A pending transfer recovers after the
 * official 500-poll wait limit. Null inputs are ignored.
 *
 * @param[in,out] forwarder Command forwarder to advance.
 * @param[in,out] transport Shared command transport used by probes and writes.
 */
void wheel_command_forwarder_run(WheelCommandForwarder *forwarder, CommandTransport *transport);

#endif
