#include "wheel/adapter_commands.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Adapter command transport layout, status bits, and scheduling values.
 *
 * These private constants describe the two adapter endpoints and the priority order used by the
 * asynchronous command service.
 */
enum {
    WHEEL_ADAPTER_ENDPOINT_COUNT = 2,   /**< Number of supported adapter endpoints. */
    WHEEL_ADAPTER_COMMAND_OWNER = 0x42, /**< Command-transport owner identifier for this service. */
    WHEEL_ADAPTER_STATUS_OFFSET = 0x00, /**< Endpoint offset of the two-byte status read. */
    WHEEL_ADAPTER_BUTTONS_OFFSET = 0x01, /**< Endpoint offset of the button read. */
    WHEEL_ADAPTER_AXES_OFFSET = 0x02,    /**< Endpoint offset of the axis read. */
    WHEEL_ADAPTER_ROTARY_OFFSET = 0x03,  /**< Endpoint offset of the rotary read. */
    WHEEL_ADAPTER_GLYPHS_OFFSET = 0x06,  /**< Endpoint offset of the glyph write. */
    WHEEL_ADAPTER_REMOTE_TUNING_ACTIVE_OFFSET =
        0x0e, /**< Endpoint offset of the remote-tuning state write. */
    WHEEL_ADAPTER_REFRESH_STATE_OFFSET = 0x17, /**< Endpoint offset of the refresh-state write. */
    WHEEL_ADAPTER_DISPLAY_STATE_OFFSET =
        0x18, /**< Endpoint offset of the system display-state write. */
    WHEEL_ADAPTER_INTERFACE_PRESENTATION_FIRST_OFFSET =
        0x20, /**< First endpoint offset for interface presentation. */
    WHEEL_ADAPTER_INTERFACE_PRESENTATION_FIRST_MODE =
        1, /**< First accepted interface presentation mode. */
    WHEEL_ADAPTER_INTERFACE_PRESENTATION_LAST_MODE =
        3,                                   /**< Last accepted interface presentation mode. */
    WHEEL_ADAPTER_TEXT_LINE_OFFSET = 0x1a,   /**< Endpoint offset of the text-line write. */
    WHEEL_ADAPTER_REPORT_TWO_OFFSET = 0x04,  /**< Endpoint offset of standard report two. */
    WHEEL_ADAPTER_REPORT_ONE_OFFSET = 0x05,  /**< Endpoint offset of standard report one. */
    WHEEL_ADAPTER_REPORT_FOUR_OFFSET = 0x08, /**< Endpoint offset of extended report four. */
    WHEEL_ADAPTER_REPORT_FIVE_OFFSET = 0x09, /**< Endpoint offset of extended report five. */
    WHEEL_ADAPTER_REPORT_SIX_OFFSET = 0x19,  /**< Endpoint offset of extended report six. */
    WHEEL_ADAPTER_SETUP_SELECTION_OFFSET =
        0xc0,                                  /**< Endpoint offset of the setup-selection write. */
    WHEEL_ADAPTER_HOST_CONTROLS_OFFSET = 0xa0, /**< Endpoint offset of the host-control read. */
    WHEEL_ADAPTER_PROBE_OFFSET = 0x0c,         /**< Endpoint offset of the endpoint probe. */
    WHEEL_ADAPTER_DISPLAY_OFFSET = 0x0f,       /**< Endpoint offset of the display write. */
    WHEEL_ADAPTER_BUTTONS_CHANGED = 0x01,      /**< Status bit requesting a button read. */
    WHEEL_ADAPTER_AXES_CHANGED = 0x02,         /**< Status bit requesting an axis read. */
    WHEEL_ADAPTER_ROTARY_CHANGED = 0x04,       /**< Status bit requesting a rotary read. */
    WHEEL_ADAPTER_HOST_CONTROLS_REQUESTED = 0x10, /**< Status bit requesting host controls. */
    WHEEL_ADAPTER_GLYPHS_REQUESTED = 0x20,        /**< Status bit requesting a glyph write. */
    WHEEL_ADAPTER_INPUT_INCREMENT = 0x04, /**< Input-status bit for one positive primary step. */
    WHEEL_ADAPTER_INPUT_DECREMENT = 0x08, /**< Input-status bit for one negative primary step. */
    WHEEL_ADAPTER_SECURE_PROFILE = 0x80, /**< Status bit suppressing ordinary component requests. */
    WHEEL_ADAPTER_OUTPUT_REPORT_INTERVAL =
        5, /**< Number of scheduling passes between report batches. */
    WHEEL_ADAPTER_COMMAND_WAIT_LIMIT = 500,
};

/** @brief Remote target identifiers indexed by adapter endpoint. */
static const uint8_t endpoint_targets[WHEEL_ADAPTER_ENDPOINT_COUNT] = {0x15, 0x16};
/** @brief Probe response lengths indexed by adapter endpoint. */
static const uint8_t endpoint_probe_lengths[WHEEL_ADAPTER_ENDPOINT_COUNT] = {1, 4};
/** @brief Rotary response lengths indexed by adapter endpoint. */
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
 * Releases the local command owner, clears incomplete input and endpoint-specific output work,
 * retains a requested system display state, marks the adapter disconnected, and selects the other
 * supported adapter mode.
 *
 * @param[in,out] service Adapter command service restarting discovery.
 * @param[in,out] adapter Logical adapter state to disconnect.
 * @param[in,out] transport Shared command transport held by the service.
 */
static void advance_endpoint(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                             CommandTransport *transport) {
    if (service->phase == WHEEL_ADAPTER_COMMAND_DISPLAY_STATE_PENDING) {
        service->display_state_pending = true;
    }
    command_transport_release(transport, WHEEL_ADAPTER_COMMAND_OWNER);
    service->endpoint_index =
        (uint8_t)((service->endpoint_index + 1u) % WHEEL_ADAPTER_ENDPOINT_COUNT);
    service->pending_inputs = 0;
    service->glyphs_pending = false;
    service->display_pending = false;
    service->text_lines_pending = 0;
    service->text_close_pending = false;
    service->host_controls_pending = false;
    service->host_controls_ready = false;
    service->interface_presentation_pending = false;
    service->remote_tuning_active_pending = false;
    service->refresh_state_pending = false;
    service->setup_selection_pending = false;
    service->status_ready = false;
    service->report_one_pending = false;
    service->report_two_pending = false;
    service->report_four_pending = false;
    service->report_five_pending = false;
    service->report_six_pending = false;
    service->output_report_cadence = 0;
    service->wait_calls = 0;
    service->output_reports_due = false;
    service->phase = WHEEL_ADAPTER_COMMAND_DISCOVERING;
    adapter->mode = service->endpoint_index;
    adapter->profile_flags = 0;
    for (uint8_t index = 0; index < sizeof(adapter->buttons); index++) {
        adapter->buttons[index] = 0;
    }
    adapter->axes[0] = 0x7f;
    adapter->axes[1] = 0x80;
    for (uint8_t index = 0; index < sizeof(adapter->rotary_positions); index++) {
        adapter->rotary_positions[index] = 0;
    }
    adapter->primary_delta = 0;
    adapter->buttons_active = false;
    for (uint8_t index = 0; index < sizeof(adapter->firmware_version); index++) {
        adapter->firmware_version[index] = 0;
    }
    for (uint8_t index = 0; index < sizeof(adapter->information); index++) {
        adapter->information[index] = 0;
    }
    adapter->connected = false;
}

/**
 * @brief Applies the latest adapter input-status response.
 *
 * Retains the profile flags, coalesces changed button, axis, rotary, host-control, and glyph work,
 * and records one signed primary motion step with increment taking priority over decrement.
 * Secure-profile status is retained without scheduling the normal component transfers.
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
        service->host_controls_pending |=
            (adapter->profile_flags & WHEEL_ADAPTER_HOST_CONTROLS_REQUESTED) != 0;
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
 * component work, and releases the local owner. A rejected transfer retains an interrupted system
 * display state and starts discovery on the alternate endpoint.
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
        service->wait_calls++;
        if (service->wait_calls > WHEEL_ADAPTER_COMMAND_WAIT_LIMIT) {
            command_transport_fail(transport);
            advance_endpoint(service, adapter, transport);
            return true;
        }
        return false;
    }
    service->wait_calls = 0;
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        advance_endpoint(service, adapter, transport);
        return true;
    }

    switch (service->phase) {
    case WHEEL_ADAPTER_COMMAND_PROBE_PENDING: {
        uint8_t status = service->probe[0];
        if (service->endpoint_index == 1) {
            status &= 0x3fu;
            for (uint8_t index = 0; index < sizeof(adapter->information); index++) {
                adapter->information[index] = service->probe[index];
            }
            adapter->firmware_version[0] = status;
            adapter->firmware_version[1] = service->probe[1];
            adapter->firmware_version[2] = service->probe[2];
        }
        adapter->connected = status != 0;
        break;
    }
    case WHEEL_ADAPTER_COMMAND_STATUS_PENDING:
        apply_status(service, adapter);
        service->status_ready = true;
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
    case WHEEL_ADAPTER_COMMAND_HOST_CONTROLS_PENDING:
        service->host_controls_pending = false;
        service->host_controls_ready = true;
        break;
    case WHEEL_ADAPTER_COMMAND_INTERFACE_PRESENTATION_PENDING:
    case WHEEL_ADAPTER_COMMAND_REMOTE_TUNING_ACTIVE_PENDING:
    case WHEEL_ADAPTER_COMMAND_REFRESH_STATE_PENDING:
    case WHEEL_ADAPTER_COMMAND_SETUP_SELECTION_PENDING:
    case WHEEL_ADAPTER_COMMAND_DISPLAY_STATE_PENDING:
    case WHEEL_ADAPTER_COMMAND_GLYPHS_PENDING:
    case WHEEL_ADAPTER_COMMAND_DISPLAY_PENDING:
    case WHEEL_ADAPTER_COMMAND_TEXT_LINE_PENDING:
    case WHEEL_ADAPTER_COMMAND_REPORT_TWO_PENDING:
    case WHEEL_ADAPTER_COMMAND_REPORT_ONE_PENDING:
    case WHEEL_ADAPTER_COMMAND_REPORT_FOUR_PENDING:
    case WHEEL_ADAPTER_COMMAND_REPORT_FIVE_PENDING:
    case WHEEL_ADAPTER_COMMAND_REPORT_SIX_PENDING:
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
 * Probes an undiscovered endpoint first, then reads changed inputs and host controls before
 * writing pending interface, control, display, text, and output-report payloads. Output-report
 * writes for the active endpoint are released together every fifth scheduling pass. When no output
 * is due, the service requests the two-byte input status. System display state waits for one
 * successful input-status response before its standard-endpoint write.
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
    if (service->host_controls_pending && !service->host_controls_ready) {
        CommandTransportResult result = command_transport_queue_read_from(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_HOST_CONTROLS_OFFSET,
            service->host_controls, sizeof(service->host_controls));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_ADAPTER_COMMAND_HOST_CONTROLS_PENDING;
        }
        return result;
    }
    if (service->interface_presentation_pending && service->endpoint_index == 1) {
        CommandTransportResult result =
            command_transport_queue_write_to(transport, WHEEL_ADAPTER_COMMAND_OWNER, target,
                                             service->interface_presentation_offset, 0, 0);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->interface_presentation_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_INTERFACE_PRESENTATION_PENDING;
        }
        return result;
    }
    if (service->remote_tuning_active_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target,
            WHEEL_ADAPTER_REMOTE_TUNING_ACTIVE_OFFSET, &service->remote_tuning_active,
            sizeof(service->remote_tuning_active));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->remote_tuning_active_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_REMOTE_TUNING_ACTIVE_PENDING;
        }
        return result;
    }
    if (service->refresh_state_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_REFRESH_STATE_OFFSET,
            &service->refresh_state, sizeof(service->refresh_state));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->refresh_state_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_REFRESH_STATE_PENDING;
        }
        return result;
    }
    if (service->setup_selection_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_SETUP_SELECTION_OFFSET,
            &service->setup_selection, sizeof(service->setup_selection));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->setup_selection_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_SETUP_SELECTION_PENDING;
        }
        return result;
    }
    if (service->display_state_pending && service->endpoint_index == 0 && service->status_ready) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_DISPLAY_STATE_OFFSET,
            &service->display_state, sizeof(service->display_state));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->display_state_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_DISPLAY_STATE_PENDING;
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
    if (service->text_lines_pending != 0 && service->endpoint_index == 1) {
        uint8_t index = 0;
        while ((service->text_lines_pending & (uint8_t)(1u << index)) == 0) {
            index++;
        }
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_TEXT_LINE_OFFSET,
            service->text_lines[index], service->text_line_lengths[index]);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->text_lines_pending &= (uint8_t)~(1u << index);
            service->phase = WHEEL_ADAPTER_COMMAND_TEXT_LINE_PENDING;
        }
        return result;
    }
    if (service->text_close_pending && service->endpoint_index == 1) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_TEXT_LINE_OFFSET,
            service->text_close, sizeof(service->text_close));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->text_close_pending = false;
            service->phase = WHEEL_ADAPTER_COMMAND_TEXT_LINE_PENDING;
        }
        return result;
    }
    bool endpoint_reports_pending = service->endpoint_index == 0
                                        ? service->report_two_pending || service->report_one_pending
                                        : service->report_four_pending ||
                                              service->report_five_pending ||
                                              service->report_six_pending;
    if (endpoint_reports_pending && !service->output_reports_due) {
        service->output_reports_due = service->output_report_cadence == 0;
        service->output_report_cadence++;
        if (service->output_report_cadence == WHEEL_ADAPTER_OUTPUT_REPORT_INTERVAL) {
            service->output_report_cadence = 0;
        }
    }
    if (service->output_reports_due && service->endpoint_index == 0 &&
        service->report_two_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_REPORT_TWO_OFFSET,
            service->report_two, sizeof(service->report_two));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->report_two_pending = false;
            service->output_reports_due = service->report_one_pending;
            service->phase = WHEEL_ADAPTER_COMMAND_REPORT_TWO_PENDING;
        }
        return result;
    }
    if (service->output_reports_due && service->endpoint_index == 0 &&
        service->report_one_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_REPORT_ONE_OFFSET,
            service->report_one, sizeof(service->report_one));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->report_one_pending = false;
            service->output_reports_due = service->report_two_pending;
            service->phase = WHEEL_ADAPTER_COMMAND_REPORT_ONE_PENDING;
        }
        return result;
    }
    if (service->output_reports_due && service->endpoint_index == 1 &&
        service->report_four_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_REPORT_FOUR_OFFSET,
            service->report_four, sizeof(service->report_four));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->report_four_pending = false;
            service->output_reports_due =
                service->report_five_pending || service->report_six_pending;
            service->phase = WHEEL_ADAPTER_COMMAND_REPORT_FOUR_PENDING;
        }
        return result;
    }
    if (service->output_reports_due && service->endpoint_index == 1 &&
        service->report_five_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_REPORT_FIVE_OFFSET,
            service->report_five, sizeof(service->report_five));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->report_five_pending = false;
            service->output_reports_due =
                service->report_four_pending || service->report_six_pending;
            service->phase = WHEEL_ADAPTER_COMMAND_REPORT_FIVE_PENDING;
        }
        return result;
    }
    if (service->output_reports_due && service->endpoint_index == 1 &&
        service->report_six_pending) {
        CommandTransportResult result = command_transport_queue_write_to(
            transport, WHEEL_ADAPTER_COMMAND_OWNER, target, WHEEL_ADAPTER_REPORT_SIX_OFFSET,
            service->report_four, 2);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->report_six_pending = false;
            service->output_reports_due =
                service->report_four_pending || service->report_five_pending;
            service->phase = WHEEL_ADAPTER_COMMAND_REPORT_SIX_PENDING;
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
 * Stores a nonzero report as two little-endian bytes followed by the fixed zero byte used by the
 * adapter display command. A newer report replaces an older queued value.
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
 * @brief Retains the adapter remote-tuning active state.
 *
 * Stores the newest Boolean state as a one-byte write to adapter offset 0x0E. A newer state
 * replaces an older queued value.
 *
 * @param[in,out] service Adapter command service retaining the state.
 * @param[in] active State sent to the adapter.
 */
void wheel_adapter_command_service_queue_remote_tuning_active(WheelAdapterCommandService *service,
                                                              bool active) {
    if (service == 0) {
        return;
    }
    service->remote_tuning_active = active ? 1u : 0u;
    service->remote_tuning_active_pending = true;
}

/**
 * @brief Queues an extended-adapter legacy interface presentation command.
 *
 * Modes one through three select zero-length writes to offsets 0x20 through 0x22. A newer valid
 * mode replaces an earlier queued presentation command. Unsupported values clear no state.
 *
 * @param[in,out] service Adapter command service retaining the command.
 * @param[in] mode Requested legacy host-interface presentation mode.
 */
void wheel_adapter_command_service_queue_interface_presentation(WheelAdapterCommandService *service,
                                                                uint8_t mode) {
    if (service == 0 || mode < WHEEL_ADAPTER_INTERFACE_PRESENTATION_FIRST_MODE ||
        mode > WHEEL_ADAPTER_INTERFACE_PRESENTATION_LAST_MODE) {
        return;
    }
    service->interface_presentation_offset =
        (uint8_t)(WHEEL_ADAPTER_INTERFACE_PRESENTATION_FIRST_OFFSET + mode - 1u);
    service->interface_presentation_pending = true;
}

/**
 * @brief Retains the adapter refresh state.
 *
 * Stores the newest Boolean state as a one-byte write to adapter offset 0x17. A newer state
 * replaces an older queued value.
 *
 * @param[in,out] service Adapter command service retaining the state.
 * @param[in] active State sent to the adapter.
 */
void wheel_adapter_command_service_queue_refresh_state(WheelAdapterCommandService *service,
                                                       bool active) {
    if (service == 0) {
        return;
    }
    service->refresh_state = active ? 1u : 0u;
    service->refresh_state_pending = true;
}

/**
 * @brief Retains a remote setup selection for the adapter.
 *
 * Stores the newest one-based setup selection for a one-byte write to adapter offset 0xC0. A
 * newer selection replaces an older queued value.
 *
 * @param[in,out] service Adapter command service retaining the selection.
 * @param[in] selection One-based setup selection.
 */
void wheel_adapter_command_service_queue_setup_selection(WheelAdapterCommandService *service,
                                                         uint8_t selection) {
    if (service == 0 || selection == 0) {
        return;
    }
    service->setup_selection = selection;
    service->setup_selection_pending = true;
}

/**
 * @brief Retains a system display state for the standard adapter endpoint.
 *
 * Stores the newest nonzero state for a one-byte write to adapter offset 0x18. A newer state
 * replaces an older queued value, while the extended endpoint leaves it pending.
 *
 * @param[in,out] service Adapter command service retaining the state.
 * @param[in] state Nonzero system display state.
 */
void wheel_adapter_command_service_queue_display_state(WheelAdapterCommandService *service,
                                                       uint8_t state) {
    if (service == 0 || state == 0) {
        return;
    }
    service->display_state = state;
    service->display_state_pending = true;
}

/**
 * @brief Queues one extended-adapter text line.
 *
 * Builds the offset-0x1A line record from a one-based line identifier, metadata byte, text length,
 * and at most 27 text bytes. Replacing a pending identifier keeps the other lines queued.
 *
 * @param[in,out] service Adapter command service retaining the line.
 * @param[in] line One-based display line identifier from one through four.
 * @param[in] metadata Display line presentation metadata.
 * @param[in] text Text bytes to retain.
 * @param[in] length Number of text bytes.
 * @return true when service and text are nonnull, the line and length are valid, and the record was
 * retained; false otherwise.
 */
bool wheel_adapter_command_service_queue_text_line(WheelAdapterCommandService *service,
                                                   uint8_t line, uint8_t metadata,
                                                   const uint8_t *text, uint8_t length) {
    if (service == 0 || text == 0 || line == 0 || line > WHEEL_ADAPTER_TEXT_LINE_COUNT ||
        length > WHEEL_ADAPTER_TEXT_LENGTH_MAXIMUM) {
        return false;
    }
    uint8_t index = line - 1u;
    service->text_lines[index][0] = line;
    service->text_lines[index][1] = metadata;
    service->text_lines[index][2] = length;
    for (uint8_t text_index = 0; text_index < length; text_index++) {
        service->text_lines[index][text_index + 3u] = text[text_index];
    }
    service->text_line_lengths[index] = length + 3u;
    service->text_lines_pending |= (uint8_t)(1u << index);
    return true;
}

/**
 * @brief Queues the extended-adapter text-page close record.
 *
 * Retains line identifier zero with standard metadata and one blank byte for offset 0x1A.
 *
 * @param[in,out] service Adapter command service retaining the close record.
 */
void wheel_adapter_command_service_queue_text_close(WheelAdapterCommandService *service) {
    if (service == 0) {
        return;
    }
    service->text_close[0] = 0;
    service->text_close[1] = 0x10;
    service->text_close[2] = 1;
    service->text_close[3] = ' ';
    service->text_close_pending = true;
}

/**
 * @brief Retains host output report one for the standard adapter endpoint.
 *
 * Copies all 12 payload bytes for a write to adapter offset 0x05. A newer report replaces an
 * older queued value.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-one payload.
 */
void wheel_adapter_command_service_queue_report_one(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_ONE_SIZE]) {
    if (service == 0 || report == 0) {
        return;
    }
    for (uint8_t index = 0; index < sizeof(service->report_one); index++) {
        service->report_one[index] = report[index];
    }
    service->report_one_pending = true;
}

/**
 * @brief Retains host output report two for the standard adapter endpoint.
 *
 * Copies all 18 payload bytes for a write to adapter offset 0x04. A newer report replaces an
 * older queued value.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-two payload.
 */
void wheel_adapter_command_service_queue_report_two(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_TWO_SIZE]) {
    if (service == 0 || report == 0) {
        return;
    }
    for (uint8_t index = 0; index < sizeof(service->report_two); index++) {
        service->report_two[index] = report[index];
    }
    service->report_two_pending = true;
}

/**
 * @brief Retains host output report four for the extended adapter endpoint.
 *
 * Copies all 25 payload bytes for a write to adapter offset 0x08. A newer report replaces an
 * older queued value.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-four payload.
 */
void wheel_adapter_command_service_queue_report_four(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_FOUR_SIZE]) {
    if (service == 0 || report == 0) {
        return;
    }
    for (uint8_t index = 0; index < sizeof(service->report_four); index++) {
        service->report_four[index] = report[index];
    }
    service->report_four_pending = true;
}

/**
 * @brief Retains host output report five for the extended adapter endpoint.
 *
 * Copies all 16 payload bytes for a write to adapter offset 0x09. A newer report replaces an
 * older queued value.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-five payload.
 */
void wheel_adapter_command_service_queue_report_five(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_FIVE_SIZE]) {
    if (service == 0 || report == 0) {
        return;
    }
    for (uint8_t index = 0; index < sizeof(service->report_five); index++) {
        service->report_five[index] = report[index];
    }
    service->report_five_pending = true;
}

/**
 * @brief Retains report-six fields for the extended adapter endpoint.
 *
 * Replaces the first two bytes of the shared report-four payload and queues those bytes for a
 * write to adapter offset 0x19. Pending report-four and report-six writes observe the same newest
 * values.
 *
 * @param[in,out] service Adapter command service retaining the report fields.
 * @param[in] first First shared report byte.
 * @param[in] second Second shared report byte.
 */
void wheel_adapter_command_service_queue_report_six(WheelAdapterCommandService *service,
                                                    uint8_t first, uint8_t second) {
    if (service == 0) {
        return;
    }
    service->report_four[0] = first;
    service->report_four[1] = second;
    service->report_six_pending = true;
}

/**
 * @brief Takes the latest adapter-originated host control batch.
 *
 * Copies the complete 30-byte control area after its offset-0xA0 read has completed and releases
 * the retained batch so a later adapter request can replace it.
 *
 * @param[in,out] service Adapter command service retaining the completed control area.
 * @param[out] output Destination for the complete control area.
 * @return true when a completed batch was copied; false when service, output, or the ready latch
 * is unavailable.
 */
bool wheel_adapter_command_service_take_host_controls(
    WheelAdapterCommandService *service, uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]) {
    if (service == 0 || output == 0 || !service->host_controls_ready) {
        return false;
    }
    for (uint8_t index = 0; index < sizeof(service->host_controls); index++) {
        output[index] = service->host_controls[index];
    }
    service->host_controls_ready = false;
    return true;
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
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        service->wait_calls = 0;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE && result != COMMAND_TRANSPORT_BUSY) {
        advance_endpoint(service, adapter, transport);
    }
}
