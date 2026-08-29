#include "wheel/command_forwarder.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    WHEEL_COMMAND_ENDPOINT_COUNT = 2,
    WHEEL_COMMAND_FORWARDER_OWNER = 0x41,
    WHEEL_COMMAND_PROBE_OFFSET = 0x0c,
    WHEEL_COMMAND_PAYLOAD_OFFSET = 0xb0,
};

static const uint8_t endpoint_targets[WHEEL_COMMAND_ENDPOINT_COUNT] = {0x15, 0x16};
static const uint8_t endpoint_probe_lengths[WHEEL_COMMAND_ENDPOINT_COUNT] = {1, 4};

/**
 * @brief Selects the current attached-device command target.
 *
 * Maps the standard endpoint to target 0x15 and the extended endpoint to target 0x16.
 *
 * @param[in] forwarder Command forwarder containing the endpoint index.
 * @return Current remote target identifier.
 */
static uint8_t endpoint_target(const WheelCommandForwarder *forwarder) {
    return endpoint_targets[forwarder->endpoint_index];
}

/**
 * @brief Restarts discovery with the alternate attached-device endpoint.
 *
 * Releases the current command-transport owner and selects the other endpoint for the next probe.
 *
 * @param[in,out] forwarder Command forwarder restarting discovery.
 * @param[in,out] transport Shared command transport held by the current endpoint.
 */
static void advance_endpoint(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    command_transport_release(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    forwarder->endpoint_index =
        (uint8_t)((forwarder->endpoint_index + 1) % WHEEL_COMMAND_ENDPOINT_COUNT);
    forwarder->phase = WHEEL_COMMAND_FORWARDER_PROBE_READY;
}

/**
 * @brief Starts discovery of the selected attached-device endpoint.
 *
 * Claims the endpoint and requests its one-byte standard or four-byte extended probe at offset
 * 0x0C. Discovery waits when another command owner is active.
 *
 * @param[in,out] forwarder Command forwarder starting a probe.
 * @param[in,out] transport Shared command transport used by the probe.
 */
static void start_probe(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    if (forwarder->payload_length == 0) {
        return;
    }

    command_transport_claim(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    if (!command_transport_is_owner(transport, WHEEL_COMMAND_FORWARDER_OWNER)) {
        return;
    }

    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(forwarder, transport);
        return;
    }

    result = command_transport_queue_read_from(transport, WHEEL_COMMAND_FORWARDER_OWNER,
                                               endpoint_target(forwarder),
                                               WHEEL_COMMAND_PROBE_OFFSET, forwarder->probe,
                                               endpoint_probe_lengths[forwarder->endpoint_index]);
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        forwarder->phase = WHEEL_COMMAND_FORWARDER_PROBE_PENDING;
    } else if (result != COMMAND_TRANSPORT_BUSY) {
        advance_endpoint(forwarder, transport);
    }
}

/**
 * @brief Completes discovery of the selected attached-device endpoint.
 *
 * Marks a successful endpoint ready. A rejected probe releases it and selects the alternate
 * endpoint.
 *
 * @param[in,out] forwarder Command forwarder awaiting a probe result.
 * @param[in,out] transport Shared command transport carrying the probe.
 */
static void finish_probe(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(forwarder, transport);
        return;
    }

    command_transport_release(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    forwarder->phase = WHEEL_COMMAND_FORWARDER_READY;
}

/**
 * @brief Starts one generic attached-device command write.
 *
 * Claims the discovered endpoint and queues the retained record batch at offset 0xB0. The local
 * batch becomes available again after the transport copies it into its request.
 *
 * @param[in,out] forwarder Command forwarder containing a pending batch.
 * @param[in,out] transport Shared command transport accepting the write.
 */
static void start_write(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    if (forwarder->payload_length == 0) {
        return;
    }

    command_transport_claim(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    if (!command_transport_is_owner(transport, WHEEL_COMMAND_FORWARDER_OWNER)) {
        return;
    }

    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(forwarder, transport);
        return;
    }

    result = command_transport_queue_write_to(
        transport, WHEEL_COMMAND_FORWARDER_OWNER, endpoint_target(forwarder),
        WHEEL_COMMAND_PAYLOAD_OFFSET, forwarder->payload, forwarder->payload_length);
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        forwarder->payload_length = 0;
        forwarder->phase = WHEEL_COMMAND_FORWARDER_WRITE_PENDING;
    } else if (result != COMMAND_TRANSPORT_BUSY) {
        advance_endpoint(forwarder, transport);
    }
}

/**
 * @brief Completes one generic attached-device command write.
 *
 * Releases a successful transfer and keeps the selected endpoint. A rejected transfer restarts
 * discovery at the alternate endpoint.
 *
 * @param[in,out] forwarder Command forwarder awaiting a write result.
 * @param[in,out] transport Shared command transport carrying the write.
 */
static void finish_write(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(forwarder, transport);
        return;
    }

    command_transport_release(transport, WHEEL_COMMAND_FORWARDER_OWNER);
    forwarder->phase = WHEEL_COMMAND_FORWARDER_READY;
}

/**
 * @brief Initializes generic attached-device command forwarding.
 *
 * Clears retained work and selects the standard endpoint for the first on-demand probe.
 *
 * @param[out] forwarder Command forwarder to initialize.
 */
void wheel_command_forwarder_init(WheelCommandForwarder *forwarder) {
    *forwarder = (WheelCommandForwarder){0};
}

/**
 * @brief Reports whether another generic command batch can be retained.
 *
 * Allows one queued batch while another copied command request is in flight.
 *
 * @param[in] forwarder Command forwarder to inspect.
 * @return True when the retained batch area is empty.
 */
bool wheel_command_forwarder_accepting(const WheelCommandForwarder *forwarder) {
    return forwarder != 0 && forwarder->payload_length == 0;
}

/**
 * @brief Queues one generic attached-device command batch.
 *
 * Retains a nonempty batch of up to 61 bytes when no other batch is waiting.
 *
 * @param[in,out] forwarder Command forwarder accepting the batch.
 * @param[in] payload Serialized complete command records.
 * @param[in] length Serialized byte count.
 * @return True when the batch was retained.
 */
bool wheel_command_forwarder_queue(WheelCommandForwarder *forwarder, const uint8_t *payload,
                                   uint8_t length) {
    if (!wheel_command_forwarder_accepting(forwarder) || payload == 0 || length == 0 ||
        length > sizeof(forwarder->payload)) {
        return false;
    }
    memcpy(forwarder->payload, payload, length);
    forwarder->payload_length = length;
    return true;
}

/**
 * @brief Advances generic attached-device command forwarding.
 *
 * Discovers the standard or extended endpoint when work first arrives, writes queued batches to
 * the selected endpoint, and restarts discovery after a rejected transfer.
 *
 * @param[in,out] forwarder Command forwarder to advance.
 * @param[in,out] transport Shared command transport used by probes and writes.
 */
void wheel_command_forwarder_run(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    if (forwarder == 0 || transport == 0) {
        return;
    }

    switch (forwarder->phase) {
    case WHEEL_COMMAND_FORWARDER_PROBE_READY:
        start_probe(forwarder, transport);
        break;
    case WHEEL_COMMAND_FORWARDER_PROBE_PENDING:
        finish_probe(forwarder, transport);
        break;
    case WHEEL_COMMAND_FORWARDER_READY:
        start_write(forwarder, transport);
        break;
    case WHEEL_COMMAND_FORWARDER_WRITE_PENDING:
        finish_write(forwarder, transport);
        break;
    }
}
