#include "wheel/adapter_commands.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_ADAPTER_ENDPOINT_COUNT = 2,
    WHEEL_ADAPTER_COMMAND_OWNER = 0x42,
    WHEEL_ADAPTER_STATUS_OFFSET = 0x00,
    WHEEL_ADAPTER_BUTTONS_OFFSET = 0x01,
    WHEEL_ADAPTER_AXES_OFFSET = 0x02,
    WHEEL_ADAPTER_ROTARY_OFFSET = 0x03,
    WHEEL_ADAPTER_GLYPHS_OFFSET = 0x06,
    WHEEL_ADAPTER_PROBE_OFFSET = 0x0c,
    WHEEL_ADAPTER_DISPLAY_OFFSET = 0x0f,
    WHEEL_ADAPTER_BUTTONS_CHANGED = 0x01,
    WHEEL_ADAPTER_AXES_CHANGED = 0x02,
    WHEEL_ADAPTER_ROTARY_CHANGED = 0x04,
    WHEEL_ADAPTER_GLYPHS_REQUESTED = 0x20,
    WHEEL_ADAPTER_INPUT_INCREMENT = 0x04,
    WHEEL_ADAPTER_INPUT_DECREMENT = 0x08,
    WHEEL_ADAPTER_SECURE_PROFILE = 0x80,
};

static const uint8_t endpoint_targets[WHEEL_ADAPTER_ENDPOINT_COUNT] = {0x15, 0x16};
static const uint8_t endpoint_probe_lengths[WHEEL_ADAPTER_ENDPOINT_COUNT] = {1, 4};
static const uint8_t endpoint_rotary_lengths[WHEEL_ADAPTER_ENDPOINT_COUNT] = {1, 2};

/**
 * @brief Selects the active adapter command target.
 *
 * Maps adapter mode zero to target 0x15 and mode one to target 0x16.
 *
 * @param[in] service Adapter command service containing the endpoint index.
 * @return Current remote target identifier.
 */
static uint8_t endpoint_target(const WheelAdapterCommandService *service) {
    return endpoint_targets[service->endpoint_index];
}

/**
 * @brief Restarts adapter discovery on the alternate endpoint.
 *
 * Releases the local command owner, clears incomplete input and output work, marks the adapter
 * disconnected, and selects the other supported adapter mode.
 *
 * @param[in,out] service Adapter command service restarting discovery.
 * @param[in,out] adapter Logical adapter state to disconnect.
 * @param[in,out] transport Shared command transport held by the service.
 */
static void advance_endpoint(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                             CommandTransport *transport) {
    command_transport_release(transport, WHEEL_ADAPTER_COMMAND_OWNER);
    service->endpoint_index =
        (uint8_t)((service->endpoint_index + 1u) % WHEEL_ADAPTER_ENDPOINT_COUNT);
    service->pending_inputs = 0;
    service->glyphs_pending = false;
    service->display_pending = false;
    service->phase = WHEEL_ADAPTER_COMMAND_DISCOVERING;
    adapter->mode = service->endpoint_index;
    adapter->profile_flags = 0;
    adapter->connected = false;
}

/**
 * @brief Applies the latest adapter input-status response.
 *
 * Retains the profile flags, coalesces changed button, axis, rotary, and glyph work, and records
 * one signed primary motion step with increment taking priority over decrement. Secure-profile
 * status is retained without scheduling the normal component transfers.
 *
 * @param[in,out] service Adapter command service containing the received status bytes.
 * @param[in,out] adapter Logical adapter state receiving flags and motion.
 */
static void apply_status(WheelAdapterCommandService *service, WheelAdapterInput *adapter) {
    adapter->profile_flags = service->status[0];
    if ((adapter->profile_flags & WHEEL_ADAPTER_SECURE_PROFILE) == 0) {
        service->pending_inputs |=
            adapter->profile_flags & (WHEEL_ADAPTER_BUTTONS_CHANGED | WHEEL_ADAPTER_AXES_CHANGED |
                                      WHEEL_ADAPTER_ROTARY_CHANGED);
        service->glyphs_pending |= (adapter->profile_flags & WHEEL_ADAPTER_GLYPHS_REQUESTED) != 0;
    }
    if ((service->status[1] & WHEEL_ADAPTER_INPUT_INCREMENT) != 0) {
        adapter->primary_delta = (int8_t)((uint8_t)adapter->primary_delta + 1u);
    } else if ((service->status[1] & WHEEL_ADAPTER_INPUT_DECREMENT) != 0) {
        adapter->primary_delta = (int8_t)((uint8_t)adapter->primary_delta - 1u);
    }
}

/**
 * @brief Completes the active adapter command.
 *
 * Waits for the shared transport, applies a successful probe or status result, clears completed
 * component work, and releases the local owner. A rejected transfer starts discovery on the
 * alternate endpoint.
 *
 * @param[in,out] service Adapter command service awaiting a result.
 * @param[in,out] adapter Logical adapter state receiving completed data.
 * @param[in,out] transport Shared command transport carrying the request.
 * @return True after a terminal result is handled; otherwise false while the request is busy.
 */
static bool finish_request(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                           CommandTransport *transport) {
    CommandTransportResult result = command_transport_poll(transport, WHEEL_ADAPTER_COMMAND_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return false;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(service, adapter, transport);
        return true;
    }

    switch (service->phase) {
    case WHEEL_ADAPTER_COMMAND_PROBE_PENDING: {
        uint8_t status = service->probe[0];
        if (service->endpoint_index == 1) {
            status &= 0x3fu;
        }
        adapter->connected = status != 0;
        break;
    }
    case WHEEL_ADAPTER_COMMAND_STATUS_PENDING:
        apply_status(service, adapter);
        break;
    case WHEEL_ADAPTER_COMMAND_BUTTONS_PENDING:
        service->pending_inputs &= (uint8_t)~WHEEL_ADAPTER_BUTTONS_CHANGED;
        break;
    case WHEEL_ADAPTER_COMMAND_AXES_PENDING:
        service->pending_inputs &= (uint8_t)~WHEEL_ADAPTER_AXES_CHANGED;
        break;
    case WHEEL_ADAPTER_COMMAND_ROTARY_PENDING:
        service->pending_inputs &= (uint8_t)~WHEEL_ADAPTER_ROTARY_CHANGED;
        break;
    case WHEEL_ADAPTER_COMMAND_GLYPHS_PENDING:
    case WHEEL_ADAPTER_COMMAND_DISPLAY_PENDING:
    case WHEEL_ADAPTER_COMMAND_DISCOVERING:
    case WHEEL_ADAPTER_COMMAND_READY:
        break;
    }

    command_transport_release(transport, WHEEL_ADAPTER_COMMAND_OWNER);
    service->phase = WHEEL_ADAPTER_COMMAND_READY;
    return true;
}

/**
 * @brief Queues the next adapter command by priority.
 *
 * Probes an undiscovered endpoint first, then reads changed buttons, axes, or selectors, writes
 * requested glyphs or a pending standard-endpoint display report, and otherwise requests the
 * two-byte input status.
 *
 * @param[in,out] service Adapter command service selecting work.
 * @param[in,out] adapter Logical adapter state receiving read results.
 * @param[in,out] transport Shared command transport accepting the request.
 * @return Complete when a request was queued, busy when unavailable, or another request error.
 */
static CommandTransportResult queue_request(WheelAdapterCommandService *service,
                                            WheelAdapterInput *adapter,
                                            CommandTransport *transport) {
    uint8_t target = endpoint_target(service);
    if (service->phase == WHEEL_ADAPTER_COMMAND_DISCOVERING) {
        CommandTransportResult result = command_transport_queue_read_from(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_PROBE_OFFSET,
            service->probe, endpoint_probe_lengths[service->endpoint_index]);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_ADAPTER_COMMAND_PROBE_PENDING;
        }
        return result;
    }
    if ((service->pending_inputs & WHEEL_ADAPTER_BUTTONS_CHANGED) != 0) {
        CommandTransportResult result = command_transport_queue_read_from(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_BUTTONS_OFFSET,
            adapter->buttons, sizeof(adapter->buttons));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_ADAPTER_COMMAND_BUTTONS_PENDING;
        }
        return result;
    }
    if ((service->pending_inputs & WHEEL_ADAPTER_AXES_CHANGED) != 0) {
        CommandTransportResult result = command_transport_queue_read_from(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_AXES_OFFSET,
            adapter->axes, sizeof(adapter->axes));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_ADAPTER_COMMAND_AXES_PENDING;
        }
        return result;
    }
    if ((service->pending_inputs & WHEEL_ADAPTER_ROTARY_CHANGED) != 0) {
        for (uint8_t index = endpoint_rotary_lengths[service->endpoint_index];
             index < WHEEL_ADAPTER_ROTARY_COUNT; index++) {
            adapter->rotary_positions[index] = 0;
        }
        CommandTransportResult result = command_transport_queue_read_from(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_ROTARY_OFFSET,
            adapter->rotary_positions, endpoint_rotary_lengths[service->endpoint_index]);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_ADAPTER_COMMAND_ROTARY_PENDING;
        }
        return result;
    }
    if (service->glyphs_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_GLYPHS_OFFSET,
            service->glyphs, sizeof(service->glyphs));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->glyphs_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_GLYPHS_PENDING;
        }
        return result;
    }
    if (service->display_pending && service->endpoint_index == 0) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_DISPLAY_OFFSET,
            service->display, sizeof(service->display));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->display_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_DISPLAY_PENDING;
        }
        return result;
    }

    CommandTransportResult result = command_transport_queue_read_from(
        transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_STATUS_OFFSET,
        service->status, sizeof(service->status));
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        service->phase = WHEEL_ADAPTER_COMMAND_STATUS_PENDING;
    }
    return result;
}

/**
 * @brief Initializes adapter command polling and logical adapter defaults.
 *
 * Selects the standard endpoint for discovery, clears queued command work, centers both adapter
 * axes at their protocol defaults, and starts disconnected.
 *
 * @param[out] service Adapter command service to initialize.
 * @param[out] adapter Logical adapter state to initialize.
 */
void wheel_adapter_command_service_init(WheelAdapterCommandService *service,
                                        WheelAdapterInput *adapter) {
    *service = (WheelAdapterCommandService){0};
    *adapter = (WheelAdapterInput){0};
    adapter->axes[0] = 0x7f;
    adapter->axes[1] = 0x80;
}

/**
 * @brief Retains the newest adapter display report for transmission.
 *
 * Stores a changed nonzero report as two little-endian bytes followed by the fixed zero byte used
 * by the adapter display command. A newer report replaces an older queued value.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Nonzero two-byte display report.
 */
void wheel_adapter_command_service_queue_display(WheelAdapterCommandService *service,
                                                 uint16_t report) {
    if (service == 0 || report == 0) {
        return;
    }
    service->display[0] = (uint8_t)report;
    service->display[1] = (uint8_t)(report >> 8);
    service->display[2] = 0;
    service->display_pending = true;
}

/**
 * @brief Updates the glyph response available to the adapter.
 *
 * Retains the three glyph bytes most recently published by the adapter-oriented packet response.
 * A status request controls when the retained bytes are written to the adapter.
 *
 * @param[in,out] service Adapter command service retaining the glyphs.
 * @param[in] glyphs Three published display glyph bytes.
 */
void wheel_adapter_command_service_set_glyphs(WheelAdapterCommandService *service,
                                              const uint8_t glyphs[3]) {
    if (service == 0 || glyphs == 0) {
        return;
    }
    for (uint8_t index = 0; index < sizeof(service->glyphs); index++) {
        service->glyphs[index] = glyphs[index];
    }
}

/**
 * @brief Advances adapter discovery, polling, and display transmission.
 *
 * Completes at most one active command per call and otherwise claims the shared transport to queue
 * the next prioritized request. Releasing after completion gives other command clients an idle
 * boundary before continuous status polling resumes.
 *
 * @param[in,out] service Adapter command service to advance.
 * @param[in,out] adapter Logical adapter state updated by completed reads.
 * @param[in,out] transport Shared command transport used for requests.
 */
void wheel_adapter_command_service_run(WheelAdapterCommandService *service,
                                       WheelAdapterInput *adapter, CommandTransport *transport) {
    if (service == 0 || adapter == 0 || transport == 0) {
        return;
    }
    if (service->phase != WHEEL_ADAPTER_COMMAND_DISCOVERING &&
        service->phase != WHEEL_ADAPTER_COMMAND_READY) {
        (void)finish_request(service, adapter, transport);
        return;
    }

    command_transport_claim(transport, WHEEL_ADAPTER_COMMAND_OWNER);
    if (!command_transport_is_owner(transport, WHEEL_ADAPTER_COMMAND_OWNER)) {
        return;
    }
    CommandTransportResult result = command_transport_poll(transport, WHEEL_ADAPTER_COMMAND_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(service, adapter, transport);
        return;
    }
    result = queue_request(service, adapter, transport);
    if (result != COMMAND_TRANSPORT_COMPLETE && result != COMMAND_TRANSPORT_BUSY) {
        advance_endpoint(service, adapter, transport);
    }
}
