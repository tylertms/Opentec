#include "pedal/service.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pedal/adjustment_probe.h"
#include "pedal/frame.h"
#include "pedal/protocol.h"
#include "pedal/v4_status.h"
#include "pedal/v4_tuning.h"
#include "platform/pedal_link.h"
#include "platform/time.h"
#include "transfer/session.h"

/**
 * @brief Pedal service protocol, timing, and retry constants.
 */
enum {
    PEDAL_DETECT_COMMAND = 0x0a,      /**< Legacy discovery request byte. */
    PEDAL_V3_PROTOCOL_COMMAND = 0x05, /**< V3 protocol query byte. */
    PEDAL_V4_PROTOCOL_COMMAND = 0x06, /**< V4 protocol query byte. */
    PEDAL_DISCOVERY_TIMEOUT_MS = 100, /**< Discovery response timeout. */
    PEDAL_PROTOCOL_TIMEOUT_MS = 101,
    PEDAL_V3_BAUD_SWITCH_DELAY_MS = 5,       /**< Delay before V3 framed receive. */
    PEDAL_INITIAL_SAMPLE_TIMEOUT_MS = 15000, /**< V3 startup sample timeout. */
    PEDAL_SAMPLE_TIMEOUT_MS = 1000,          /**< V3 active sample timeout. */
    PEDAL_STARTUP_FRAME_COUNT = 250, /**< Accepted reports required to leave startup timeout. */
    PEDAL_RECONNECT_DELAY_MS = 550,  /**< Published-input hold after a digital-link failure. */
    PEDAL_STATUS_INTERVAL_MS = 500,  /**< V3 status request interval. */
    PEDAL_INPUT_COMMAND_INTERVAL_MS = 500, /**< V3 input-command interval. */
    PEDAL_KEEPALIVE_INTERVAL_MS = 2500,    /**< V3 calibration keepalive interval. */
    PEDAL_V4_STATUS_INTERVAL_MS = 15,      /**< V4 status request interval. */
    PEDAL_V4_RESPONSE_TIMEOUT_MS = 100,    /**< V4 initial response timeout. */
    PEDAL_V4_OPERATION_TIMEOUT_MS = 20000, /**< V4 asynchronous operation timeout. */
    PEDAL_V4_KEEPALIVE_INTERVAL_MS = 100,  /**< V4 adjustment keepalive interval. */
    PEDAL_LEGACY_RESPONSE_TIMEOUT_MS = 18, /**< Legacy channel response timeout. */
    PEDAL_LEGACY_AXIS_1_RETRY_LIMIT = 5,   /**< Retry limit for the first legacy axis. */
    PEDAL_LEGACY_RETRY_LIMIT = 6,          /**< Retry limit for other legacy channels. */
    PEDAL_PROTOCOL_PRESERVE_VALUE =
        0x66, /**< Value that preserves the retained value and second selector. */
};

/**
 * @brief Response returned when a V4 host request is made without an active V4 session.
 */
static const uint8_t pedal_protocol_disabled_response[] = {
    0x0c, 0x0a, 0x02, 0x08, 0x02, 0x18, 0x08, 0xb2, 0x01, 0x03, 0xa2, 0x0b, 0x00, 0x38, 0x78,
};

/**
 * @brief Reports whether a monotonic deadline is strictly in the past.
 *
 * Uses signed subtraction so comparisons remain valid across the millisecond counter wrap.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to test.
 * @return True only after the deadline has passed.
 */
static bool time_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * @brief Publishes the selected auxiliary input source.
 *
 * Chooses the local override while it is active and otherwise restores the latest remote pedal
 * value.
 *
 * @param[in,out] service Pedal input and auxiliary source state to update.
 */
static void publish_auxiliary(PedalService *service) {
    service->input.auxiliary = service->auxiliary_override_active ? service->auxiliary_override
                                                                  : service->remote_auxiliary;
}

/**
 * @brief Releases the published digital pedal input.
 *
 * Clears remote pedal values while preserving a currently active local auxiliary override.
 *
 * @param[in,out] service Pedal input and auxiliary source state to release.
 */
static void release_published_input(PedalService *service) {
    pedal_input_release(&service->input);
    service->remote_auxiliary = 0;
    publish_auxiliary(service);
}

/**
 * @brief Clears pending V3 output operations.
 *
 * Restores the control, input-command, configuration, and periodic output state used when a V3
 * connection starts or is released.
 *
 * @param[in,out] service V3 output state to clear.
 */
static void clear_v3_outbound(PedalService *service) {
    service->pending_control = 0;
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        service->input_command[axis] = 0;
    }
    service->input_command_pending = false;
    service->configuration_pending = false;
    service->configuration_reset_pending = false;
    service->startup_handshake_active = false;
    service->status_handshake_active = false;
    service->next_input_command_ms = 0;
    service->next_keepalive_ms = 0;
}

/**
 * @brief Clears protocol state for a reconnecting digital pedal.
 *
 * Resets transport and operation state without changing published input, phase, or reconnect
 * deadline. The caller controls when the stopped link and retained input become visible.
 *
 * @param[in,out] service Pedal protocol and reconnect state to clear.
 */
static void reset_reconnect_state(PedalService *service) {
    const bool configuration_pending = service->configuration_pending;
    const bool configuration_reset_pending = service->configuration_reset_pending;
    pedal_v3_state_init(&service->v3);
    service->v4.active = false;
    clear_v3_outbound(service);
    service->configuration_pending = configuration_pending;
    service->configuration_reset_pending = configuration_reset_pending;
    service->connected = false;
    service->digital_activity = false;
    service->v4_phase = PEDAL_V4_PHASE_STATUS;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
    service->v4_response_deadline_ms = 0;
    service->v4_operation_deadline_ms = 0;
    service->next_v4_keepalive_ms = 0;
    service->alternate_brake_force_received = false;
    service->adjustment_display_pending = false;
    service->device = PEDAL_DEVICE_NONE;
    service->startup_frame_count = 0;
    service->legacy_channel = PEDAL_LEGACY_AXIS_1;
    for (uint8_t channel = 0; channel < PEDAL_LEGACY_CHANNEL_COUNT; channel++) {
        service->legacy_retries[channel] = 0;
    }
}

/**
 * @brief Releases the current pedal source and schedules digital discovery.
 *
 * Stops serial reception before clearing the current generation. Selects local analog input only
 * when no digital traffic preceded the failure. A link that has produced accepted digital traffic
 * retains its published input through the reconnect hold before discovery resumes.
 *
 * @param[in,out] service Pedal source, transport, and reconnect state to reset.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void reconnect(PedalService *service, uint32_t now_ms) {
    const bool retain_published_input = service->digital_activity;
    platform_pedal_link_stop_receive();
    if (!retain_published_input) {
        release_published_input(service);
    }
    reset_reconnect_state(service);
    if (!retain_published_input && service->analog_samples_ready &&
        pedal_analog_detect(service->analog_samples) &&
        pedal_analog_update(&service->analog, service->analog_samples, &service->input)) {
        service->remote_auxiliary = service->input.auxiliary;
        publish_auxiliary(service);
        platform_pedal_link_begin_analog();
        service->connected = true;
        service->phase = PEDAL_SERVICE_ANALOG;
        return;
    }
    service->phase = PEDAL_SERVICE_RECONNECT_WAIT;
    service->deadline_ms = now_ms + (retain_published_input ? PEDAL_RECONNECT_DELAY_MS : 0);
}

/**
 * @brief Performs link setup on the first pass of a retained-input reconnect hold.
 *
 * Runs after the hold deadline is anchored, then leaves the existing deadline and wait phase for
 * the remaining hold passes.
 *
 * @param[in,out] service Pedal transport and protocol state to reset.
 */
static void setup_reconnect_link(PedalService *service) {
    platform_pedal_link_stop_receive();
    reset_reconnect_state(service);
}

/**
 * @brief Encodes and submits the current V3 transmit frame.
 *
 * Serializes the retained frame into its fixed transport buffer before starting transmission.
 *
 * @param[in,out] service V3 frame and encoded transport buffer to use.
 * @return True when the pedal link accepts the frame.
 */
static bool send_frame(PedalService *service) {
    pedal_frame_encode(&service->transmit_frame, service->frame_buffer);
    return platform_pedal_link_send_frame(service->frame_buffer);
}

/**
 * @brief Initializes the complete pedal service state.
 *
 * Releases published inputs, resets every supported transport, restores protocol defaults, and
 * leaves the phase ready for digital discovery on the first service pass.
 *
 * @param[out] service Pedal service state to initialize.
 */
void pedal_service_init(PedalService *service) {
    pedal_input_release(&service->input);
    pedal_v3_state_init(&service->v3);
    service->v4.active = false;
    pedal_analog_init(&service->analog);
    service->phase = PEDAL_SERVICE_DETECT_REQUEST;
    service->device = PEDAL_DEVICE_NONE;
    service->deadline_ms = 0;
    service->next_status_ms = 0;
    service->next_input_command_ms = 0;
    service->next_keepalive_ms = 0;
    service->v4_response_deadline_ms = 0;
    service->v4_operation_deadline_ms = 0;
    service->next_v4_keepalive_ms = 0;
    service->next_service_ms = 0;
    service->response = 0;
    service->brake_force_percent = 100;
    service->startup_frame_count = 0;
    service->configuration_brake_force = 0;
    service->v4_tuning_pending = 0;
    service->v4_sent_value = 0;
    service->remote_auxiliary = 0;
    service->auxiliary_override = 0;
    service->transfer_response = (PedalTransferResponse){0};
    service->legacy_channel = PEDAL_LEGACY_AXIS_1;
    service->protocol_status = (PedalProtocolStatus){0};
    service->transmitted_status = (PedalProtocolStatus){0};
    service->v4_tuning = (PedalV4Tuning){0};
    pedal_transfer_queue_init(&service->host_transfer_queue);
    service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
    service->v4_phase = PEDAL_V4_PHASE_STATUS;
    for (uint8_t channel = 0; channel < PEDAL_LEGACY_CHANNEL_COUNT; channel++) {
        service->legacy_retries[channel] = 0;
    }
    service->analog_samples_ready = false;
    service->connected = false;
    service->digital_activity = false;
    service->auxiliary_override_active = false;
    service->status_handshake_active = false;
    service->recovery_handshake = false;
    service->status_transmitted = false;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->alternate_brake_force_received = false;
    service->host_adjustment_pending = false;
    service->button_adjustment_pending = false;
    service->adjustment_display_pending = false;
    service->adjustment_display = PEDAL_ADJUSTMENT_DISPLAY_NONE;
    service->clock_ms = 0;
    clear_v3_outbound(service);
}

/**
 * @brief Restarts attached-pedal discovery.
 *
 * Stops the current UART/DMA receive generation, releases the published pedal source, clears the
 * active digital identity and connection, returns the transport to byte discovery, and schedules
 * the detection request immediately.
 *
 * @param[in,out] service Pedal service to restart.
 */
void pedal_service_request_startup(PedalService *service) {
    if (service == NULL) {
        return;
    }
    platform_pedal_link_stop_receive();
    pedal_input_release(&service->input);
    pedal_v3_state_init(&service->v3);
    service->v4.active = false;
    service->v4 = (TransferSession){0};
    pedal_transfer_queue_init(&service->host_transfer_queue);
    service->transfer_response = (PedalTransferResponse){0};
    clear_v3_outbound(service);
    service->v4_tuning_pending = 0;
    service->v4_sent_value = 0;
    service->v4_phase = PEDAL_V4_PHASE_STATUS;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->v4_response_deadline_ms = 0;
    service->v4_operation_deadline_ms = 0;
    service->next_v4_keepalive_ms = 0;
    service->host_adjustment_pending = false;
    service->button_adjustment_pending = false;
    service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
    service->adjustment_display_pending = false;
    service->adjustment_display = PEDAL_ADJUSTMENT_DISPLAY_NONE;
    service->alternate_brake_force_received = false;
    service->startup_frame_count = 0;
    service->startup_handshake_active = false;
    service->status_handshake_active = false;
    service->recovery_handshake = false;
    service->status_transmitted = false;
    service->remote_auxiliary = 0;
    publish_auxiliary(service);
    service->device = PEDAL_DEVICE_NONE;
    service->connected = false;
    service->digital_activity = false;
    service->phase = PEDAL_SERVICE_DETECT_REQUEST;
    service->deadline_ms = 0;
    service->next_service_ms = 0;
    platform_pedal_link_begin_discovery();
}

/**
 * @brief Submits one encoded V4 frame to the pedal link.
 *
 * Adapts the transfer session callback to the asynchronous pedal transmitter.
 *
 * @param[in] context Unused pedal service callback context.
 * @param[in] data Encoded transfer frame.
 * @param[in] length Encoded frame length.
 */
static void send_v4_transfer(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    (void)platform_pedal_link_send_transfer(data, length);
}

/**
 * @brief Reports whether the V4 pedal transmitter is occupied.
 *
 * Adapts the platform transmitter state to the V4 transfer-session callback interface.
 *
 * @param[in] context Unused pedal service callback context.
 * @return True while a prior frame is still being transmitted.
 */
static bool v4_transfer_busy(void *context) {
    (void)context;
    return platform_pedal_link_transmit_busy();
}

/**
 * @brief Retains a completed host pedal response.
 *
 * Keeps the first response of up to 124 bytes until the USB command service finishes forwarding
 * it. A later response is left unqueued while that slot remains occupied.
 *
 * @param[in,out] service Pedal service retaining the response.
 * @param[in] data Complete response payload.
 * @param[in] length Response payload length.
 * @param[in] source Operation that produced the response.
 */
static void retain_transfer_response(PedalService *service, const uint8_t *data, uint8_t length,
                                     PedalTransferResponseSource source) {
    if (service->transfer_response.length != 0 || data == NULL || length == 0 ||
        length > sizeof(service->transfer_response.data)) {
        return;
    }
    memcpy(service->transfer_response.data, data, length);
    service->transfer_response.length = length;
    service->transfer_response.source = source;
}

/**
 * @brief Accepts a completed group-zero response for the active V4 operation.
 *
 * Ignores intermediate fragments and responses outside the active pedal channel. A completed
 * status response publishes its axes, while adjustment replies are retained for the host or
 * classified for the local display according to the operation phase.
 *
 * @param[in,out] context Pedal service awaiting the response.
 * @param[in] data Response payload from the transfer session.
 * @param[in] length Response payload length.
 * @param[in] group Transfer channel group.
 * @param[in] complete True when this payload completes the response sequence.
 */
static void apply_v4_status(void *context, const uint8_t *data, uint8_t length, uint8_t group,
                            bool complete) {
    PedalService *service = context;
    bool adjustment_waiting = service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_WAIT;
    if (group != 0 || (!service->v4_request_active && !adjustment_waiting) || !complete) {
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_STATUS) {
        pedal_v4_status_parse(data, length, service->input.axes);
    } else if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_START) {
        if (service->adjustment_source == PEDAL_ADJUSTMENT_SOURCE_HOST) {
            retain_transfer_response(service, data, length, PEDAL_TRANSFER_RESPONSE_ADJUSTMENT);
        }
    } else if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_WAIT) {
        if (pedal_adjustment_probe_classify(data, length, &service->adjustment_display)) {
            service->adjustment_display_pending = true;
        }
        if (service->adjustment_source == PEDAL_ADJUSTMENT_SOURCE_HOST) {
            retain_transfer_response(service, data, length, PEDAL_TRANSFER_RESPONSE_ADJUSTMENT);
        }
    } else if (service->v4_phase == PEDAL_V4_PHASE_HOST_TRANSFER) {
        retain_transfer_response(service, data, length, PEDAL_TRANSFER_RESPONSE_HOST_REQUEST);
    }
    service->connected = true;
    service->v4_response_received = true;
}

/**
 * @brief Reads the time supplied to the current pedal service iteration.
 *
 * Adapts the service clock snapshot to the V4 transfer-session callback interface.
 *
 * @param[in] context Pedal service containing the current monotonic time.
 * @return Current monotonic time in milliseconds.
 */
static uint32_t read_v4_clock(void *context) {
    const PedalService *service = context;
    return service->clock_ms;
}

/**
 * @brief Callbacks connecting the V4 transfer session to the pedal service.
 */
static const TransferSessionCallbacks v4_callbacks = {
    .send = send_v4_transfer,
    .ready = v4_transfer_busy,
    .data = apply_v4_status,
    .clock = read_v4_clock,
};

/**
 * @brief Publishes the latest local analog pedal samples to the service.
 *
 * Retains all three channels for fallback detection and updates an active analog source. Loss of
 * a valid active source returns the service to digital discovery.
 *
 * @param[in,out] service Analog samples, input state, and source selection to update.
 * @param[in] samples Three local pedal samples in primary, secondary, and tertiary order.
 */
void pedal_service_set_analog_samples(PedalService *service,
                                      const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        service->analog_samples[axis] = samples[axis];
    }
    service->analog_samples_ready = true;
    if (service->phase == PEDAL_SERVICE_ANALOG &&
        !pedal_analog_update(&service->analog, service->analog_samples, &service->input)) {
        reconnect(service, service->clock_ms);
    } else if (service->phase == PEDAL_SERVICE_ANALOG) {
        service->remote_auxiliary = service->input.auxiliary;
        publish_auxiliary(service);
    }
}

/**
 * @brief Sets the V3 brake-force scaling control.
 *
 * Retains the full eight-bit control value so the V3 input path can apply its signed percentage
 * adjustment to subsequent brake reports.
 *
 * @param[in,out] service Pedal service containing the scaling control.
 * @param[in] force_percent Encoded brake-force control value.
 */
void pedal_service_set_brake_force(PedalService *service, uint8_t force_percent) {
    service->brake_force_percent = force_percent;
}

/**
 * @brief Records one changed V4 tuning value for transmission.
 *
 * Stores the requested value and raises the setting's pending bit only when the value changes.
 *
 * @param[in,out] current Retained value for the setting.
 * @param[in] value Requested value for the setting.
 * @param[in] setting One-based V4 tuning setting identifier.
 * @param[in,out] pending Pending-setting mask to update.
 */
static void update_v4_tuning_value(uint8_t *current, uint8_t value, PedalV4TuningSetting setting,
                                   uint8_t *pending) {
    if (*current == value) {
        return;
    }
    *current = value;
    *pending |= (uint8_t)(1u << (setting - 1));
}

/**
 * @brief Applies the current V4 pedal tuning values.
 *
 * Records brake force and pedal curves in the protocol-defined priority order when their values
 * differ from the retained configuration.
 *
 * @param[in,out] service V4 tuning values and pending-setting mask to update.
 * @param[in] tuning Requested brake-force and pedal-curve values.
 */
void pedal_service_set_v4_tuning(PedalService *service, PedalV4Tuning tuning) {
    update_v4_tuning_value(&service->v4_tuning.brake_force, tuning.brake_force,
                           PEDAL_V4_TUNING_BRAKE_FORCE, &service->v4_tuning_pending);
    update_v4_tuning_value(&service->v4_tuning.clutch_curve, tuning.clutch_curve,
                           PEDAL_V4_TUNING_CLUTCH_CURVE, &service->v4_tuning_pending);
    update_v4_tuning_value(&service->v4_tuning.brake_curve, tuning.brake_curve,
                           PEDAL_V4_TUNING_BRAKE_CURVE, &service->v4_tuning_pending);
    update_v4_tuning_value(&service->v4_tuning.throttle_curve, tuning.throttle_curve,
                           PEDAL_V4_TUNING_THROTTLE_CURVE, &service->v4_tuning_pending);
}

/**
 * @brief Reports whether the V4 pedal transfer session is active.
 *
 * Exposes the session gate used before accepting an external pedal-adjustment request.
 *
 * @param[in] service Pedal service containing the transfer session.
 * @return True while the V4 transfer session can accept an adjustment query.
 */
bool pedal_service_adjustment_available(const PedalService *service) {
    return service != 0 && service->v4.active;
}

/**
 * @brief Queues a host-requested V4 pedal adjustment.
 *
 * Gives the host operation priority over a pending wheel-button operation and leaves its first
 * completed response in the shared response slot for USB forwarding.
 *
 * @param[in,out] service Pedal service receiving the host request.
 */
void pedal_service_request_host_adjustment(PedalService *service) {
    service->host_adjustment_pending = true;
}

/**
 * @brief Queues a wheel-button V4 pedal adjustment.
 *
 * Retains the operation until its final response or 20-second operation deadline.
 *
 * @param[in,out] service Pedal service receiving the wheel-button request.
 */
void pedal_service_request_button_adjustment(PedalService *service) {
    service->button_adjustment_pending = true;
}

/**
 * @brief Queues one logical host request for the V4 pedal controller.
 *
 * Returns a fixed protocol-disabled response when no V4 session is active, silently ignores the
 * reserved 0x3D1B request, routes the 0xB6F8 request to host adjustment, and otherwise appends the
 * request to the eleven-entry FIFO when space is available.
 *
 * @param[in,out] service Pedal service receiving the request.
 * @param[in] data Complete logical request payload.
 * @param[in] length Request payload length from one through 124 bytes.
 */
void pedal_service_queue_host_transfer(PedalService *service, const uint8_t *data, uint8_t length) {
    if (service == NULL || data == NULL || length == 0 ||
        length > PEDAL_TRANSFER_PAYLOAD_CAPACITY) {
        return;
    }
    if (!service->v4.active) {
        retain_transfer_response(service, pedal_protocol_disabled_response,
                                 sizeof(pedal_protocol_disabled_response),
                                 PEDAL_TRANSFER_RESPONSE_NONE);
        return;
    }
    if (length >= 2 && data[length - 2u] == 0x3d && data[length - 1u] == 0x1b) {
        return;
    }
    if (length >= 2 && data[length - 2u] == 0xb6 && data[length - 1u] == 0xf8) {
        service->host_adjustment_pending = true;
        return;
    }
    (void)pedal_transfer_queue_push(&service->host_transfer_queue, data, length);
}

/**
 * @brief Provides the pending host pedal response.
 *
 * Keeps the response stable through every USB fragment retry.
 *
 * @param[in] service Pedal service retaining the response.
 * @return Pending response, or null when the response slot is empty.
 */
const PedalTransferResponse *pedal_service_transfer_response(const PedalService *service) {
    return service != NULL && service->transfer_response.length != 0 ? &service->transfer_response
                                                                     : NULL;
}

/**
 * @brief Releases the pending host pedal response.
 *
 * Completes the active generic request after its final USB fragment and opens the shared response
 * slot for the next pedal reply.
 *
 * @param[in,out] service Pedal service releasing the response.
 */
void pedal_service_release_transfer_response(PedalService *service) {
    if (service == NULL) {
        return;
    }
    if (service->transfer_response.source == PEDAL_TRANSFER_RESPONSE_HOST_REQUEST) {
        pedal_transfer_queue_finish(&service->host_transfer_queue);
    }
    service->transfer_response = (PedalTransferResponse){0};
}

/**
 * @brief Takes the newest pedal-adjustment display command.
 *
 * Returns each hold or response-classification command once and clears its pending indication.
 *
 * @param[in,out] service Pedal service retaining the display command.
 * @return Pending display command, or the idle value when no command was available.
 */
PedalAdjustmentDisplay pedal_service_take_adjustment_display(PedalService *service) {
    if (!service->adjustment_display_pending) {
        return PEDAL_ADJUSTMENT_DISPLAY_IDLE;
    }
    service->adjustment_display_pending = false;
    return service->adjustment_display;
}

/**
 * @brief Selects the local or remote auxiliary input source.
 *
 * Publishes the supplied local byte while active. Releasing the override immediately restores the
 * most recent auxiliary value received from the attached pedal source.
 *
 * @param[in,out] service Pedal input and auxiliary source state to update.
 * @param[in] active True when the local analog input owns the auxiliary axis.
 * @param[in] value Calibrated local auxiliary byte.
 */
void pedal_service_set_auxiliary_override(PedalService *service, bool active, uint8_t value) {
    service->auxiliary_override_active = active;
    service->auxiliary_override = value;
    publish_auxiliary(service);
}

/**
 * @brief Selects automatic auxiliary endpoint calibration from pedal state.
 *
 * Enables automatic settling while the legacy-calibration flag or either V3 calibration path is
 * active and no relevant V3 connection flag is asserted.
 *
 * @param[in] service Current pedal protocol, connection, and calibration state.
 * @return True when the auxiliary input uses automatic endpoint settling.
 */
bool pedal_service_auxiliary_automatic_calibration(const PedalService *service) {
    return pedal_service_calibration_active(service) && (service->v3.connection_flags & 0xaa) == 0;
}

/**
 * @brief Replaces the complete pedal protocol status.
 *
 * Stores the value, selectors, and scale used by legacy polling and V3 status reports.
 *
 * @param[in,out] service Pedal service and protocol status to update.
 * @param[in] status Complete replacement protocol status.
 */
void pedal_service_set_protocol_status(PedalService *service, const PedalProtocolStatus *status) {
    service->protocol_status = *status;
}

/**
 * @brief Clears the complete pedal protocol status.
 *
 * Resets the retained value, both selectors, and the legacy scale to zero.
 *
 * @param[in,out] service Pedal service receiving the cleared protocol status.
 */
void pedal_service_reset_protocol_status(PedalService *service) {
    service->protocol_status = (PedalProtocolStatus){0};
}

/**
 * @brief Applies the brake indicator to the pedal protocol selector.
 *
 * Replaces only the first protocol selector and preserves the configured value, second selector,
 * and legacy scale.
 *
 * @param[in,out] service Pedal protocol status to update.
 * @param[in] selector Active or released brake-indicator selector.
 */
void pedal_service_set_brake_indicator_selector(PedalService *service, uint8_t selector) {
    service->protocol_status.first = selector;
}

/**
 * @brief Reports whether the byte-oriented legacy pedal transport is active.
 *
 * Includes both request and response phases of the legacy polling cycle.
 *
 * @param[in] service Current pedal service phase.
 * @return True during either legacy transport phase; otherwise false.
 */
bool pedal_service_legacy_transport_active(const PedalService *service) {
    return service->phase == PEDAL_SERVICE_LEGACY_REQUEST ||
           service->phase == PEDAL_SERVICE_LEGACY_RESPONSE;
}

/**
 * @brief Reports retained legacy pedal compatibility mode.
 *
 * @param[in] service Pedal service to inspect.
 * @return True while legacy pedal compatibility is active; otherwise false.
 */
bool pedal_service_legacy_mode(const PedalService *service) {
    return service != NULL &&
           (service->phase == PEDAL_SERVICE_V3_START || service->phase == PEDAL_SERVICE_V3_STREAM);
}

/**
 * @brief Reports whether the modern pedal startup handshake is active.
 *
 * Includes the switch delay, stream-start phase, and the initial V3 frames that complete startup.
 *
 * @param[in] service Current pedal service state.
 * @return True while the V3 startup handshake is in progress.
 */
bool pedal_service_handshake_active(const PedalService *service) {
    return service != NULL &&
           (service->phase == PEDAL_SERVICE_V3_SWITCH_WAIT ||
            service->phase == PEDAL_SERVICE_V3_START ||
            (service->phase == PEDAL_SERVICE_V3_STREAM && service->startup_handshake_active));
}

bool pedal_service_extended_status_handshake_active(const PedalService *service) {
    return service != NULL && service->status_handshake_active;
}

/**
 * @brief Applies a host update to the pedal protocol status.
 *
 * Protocol updates always replace the first selector. Value 0x66 preserves the current value and
 * second selector; other values replace both. Legacy-scale updates are accepted only while the
 * legacy-calibration flag is active.
 *
 * @param[in,out] service Pedal transport and protocol status to update.
 * @param[in] command Decoded protocol tuple or legacy-scale command.
 */
void pedal_service_apply_protocol_command(PedalService *service,
                                          const PedalProtocolCommand *command) {
    if (command->kind == PEDAL_PROTOCOL_COMMAND_UPDATE) {
        service->protocol_status.first = command->first;
        if (command->value != PEDAL_PROTOCOL_PRESERVE_VALUE) {
            service->protocol_status.value = command->value;
            service->protocol_status.second = command->second;
        }
        return;
    }

    if (service->v3.legacy_calibration) {
        service->protocol_status.scale = command->value;
    }
}

/**
 * @brief Reports whether the attached pedal path accepts calibration commands.
 *
 * Accepts commands while the legacy-calibration flag or either V3 calibration mode is active.
 *
 * @param[in] service Current pedal transport and calibration state.
 * @return True when pedal calibration commands may be queued.
 */
bool pedal_service_calibration_active(const PedalService *service) {
    return service->v3.legacy_calibration || service->v3.primary_calibration ||
           service->v3.secondary_calibration;
}

/**
 * @brief Queues one or more pedal calibration controls.
 *
 * Merges the requested controls with any controls still waiting for transmission.
 *
 * @param[in,out] service Pedal service and pending control mask.
 * @param[in] control Calibration control bits to queue.
 */
void pedal_service_request_control(PedalService *service, PedalV3Control control) {
    service->pending_control |= (uint8_t)control;
}

/**
 * @brief Reports whether a pedal calibration control awaits transmission.
 *
 * Exposes completion of the existing control queue without coupling tuning interaction to the
 * pedal transport representation.
 *
 * @param[in] service Pedal service and pending control mask.
 * @return True while at least one control remains queued.
 */
bool pedal_service_control_pending(const PedalService *service) {
    return service != NULL && service->pending_control != 0;
}

/**
 * @brief Queues one three-channel pedal calibration input command.
 *
 * Replaces the pending input values with the latest host command.
 *
 * @param[in,out] service Pedal service and pending input command.
 * @param[in] values Three calibration input values to queue.
 */
void pedal_service_request_input_command(PedalService *service,
                                         const uint8_t values[PEDAL_INPUT_AXIS_COUNT]) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        service->input_command[axis] = values[axis];
    }
    service->input_command_pending = true;
}

/**
 * @brief Queues the alternate brake-force configuration for a V3 pedal controller.
 *
 * Replaces the pending percentage and retains a reset request until the configuration frame is
 * transmitted during an active calibration mode.
 *
 * @param[in,out] service Pedal service and pending configuration state to update.
 * @param[in] brake_force Alternate brake-force percentage to transmit.
 * @param[in] reset True to include the controller reset marker.
 */
void pedal_service_request_configuration(PedalService *service, uint8_t brake_force, bool reset) {
    service->configuration_brake_force = brake_force;
    service->configuration_pending = true;
    service->configuration_reset_pending |= reset;
}

/**
 * @brief Takes the latest alternate brake-force value reported by a V3 pedal controller.
 *
 * Returns each received brake-force report once and clears its pending notification.
 *
 * @param[in,out] service Pedal service and report notification state to consume.
 * @return Reported percentage, or PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE when none is pending.
 */
uint8_t pedal_service_take_alternate_brake_force(PedalService *service) {
    if (!service->alternate_brake_force_received) {
        return PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE;
    }
    service->alternate_brake_force_received = false;
    return service->v3.alternate_brake_force;
}

/**
 * @brief Accepts a supported pedal identity or ends the discovery attempt.
 *
 * Advances recognized V3 and V4 identities to protocol selection. An expired attempt releases the
 * source and schedules a fresh discovery without an established-link delay.
 *
 * @param[in,out] service Pedal discovery and reconnect state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_detect_response(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_byte(&service->response)) {
        if (service->response == PEDAL_DEVICE_V3 || service->response == PEDAL_DEVICE_V4) {
            service->device = (PedalDevice)service->response;
            service->phase = PEDAL_SERVICE_PROTOCOL_REQUEST;
            return;
        }
    }
    if (platform_time_reached(now_ms, service->deadline_ms)) {
        reconnect(service, now_ms);
    }
}

/**
 * @brief Waits for the protocol response of a recognized pedal device.
 *
 * Retains any received byte for protocol selection and restarts discovery when the response window
 * expires.
 *
 * @param[in,out] service Pedal protocol and reconnect state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_protocol_response(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_byte(&service->response)) {
        service->phase = PEDAL_SERVICE_SELECT_PROTOCOL;
        return;
    }
    if (platform_time_reached(now_ms, service->deadline_ms)) {
        reconnect(service, now_ms);
    }
}

/**
 * @brief Selects the transport implied by the device and protocol response pair.
 *
 * Routes recognized modern protocols to their startup states, assigns all other nonempty pairs to
 * legacy polling, and repeats discovery for empty or invalid pairs.
 *
 * @param[in,out] service Pedal identity, response, and selected phase to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_select_protocol(PedalService *service, uint32_t now_ms) {
    switch (pedal_protocol_select(service->device, service->response)) {
    case PEDAL_PROTOCOL_V3:
        service->phase = PEDAL_SERVICE_V3_SWITCH_WAIT;
        service->deadline_ms = now_ms + PEDAL_V3_BAUD_SWITCH_DELAY_MS;
        break;
    case PEDAL_PROTOCOL_V4:
        service->phase = PEDAL_SERVICE_V4_START;
        break;
    case PEDAL_PROTOCOL_LEGACY:
        service->legacy_channel = PEDAL_LEGACY_AXIS_1;
        service->phase = PEDAL_SERVICE_LEGACY_REQUEST;
        break;
    case PEDAL_PROTOCOL_REDISCOVER:
        service->phase = PEDAL_SERVICE_DETECT_REQUEST;
        break;
    }
}

/**
 * @brief Maps a V4 tuning setting to its pending-work bit.
 *
 * Converts the one-based setting identifier to the corresponding zero-based bit position.
 *
 * @param[in] setting One-based V4 tuning setting identifier.
 * @return Pending-work mask for the setting.
 */
static uint8_t v4_setting_mask(PedalV4TuningSetting setting) {
    return (uint8_t)(1u << (setting - 1));
}

/**
 * @brief Maps a V4 request phase to its tuning setting.
 *
 * Recognizes brake-force, clutch-curve, brake-curve, and throttle-curve request phases.
 *
 * @param[in] phase Current V4 service phase.
 * @return Matching tuning setting, or zero for non-tuning phases.
 */
static PedalV4TuningSetting v4_phase_setting(PedalV4Phase phase) {
    switch (phase) {
    case PEDAL_V4_PHASE_BRAKE_FORCE:
        return PEDAL_V4_TUNING_BRAKE_FORCE;
    case PEDAL_V4_PHASE_CLUTCH_CURVE:
        return PEDAL_V4_TUNING_CLUTCH_CURVE;
    case PEDAL_V4_PHASE_BRAKE_CURVE:
        return PEDAL_V4_TUNING_BRAKE_CURVE;
    case PEDAL_V4_PHASE_THROTTLE_CURVE:
        return PEDAL_V4_TUNING_THROTTLE_CURVE;
    case PEDAL_V4_PHASE_STATUS:
    case PEDAL_V4_PHASE_SELECT:
    case PEDAL_V4_PHASE_ADJUSTMENT_START:
    case PEDAL_V4_PHASE_ADJUSTMENT_WAIT:
    case PEDAL_V4_PHASE_HOST_TRANSFER:
        return 0;
    }
    return 0;
}

/**
 * @brief Selects the configured value for a V4 tuning setting.
 *
 * Reads the matching retained V4 setting without modifying pending-work state.
 *
 * @param[in] tuning Current V4 tuning values.
 * @param[in] setting Value to select.
 * @return Configured setting value, or zero for an unknown setting.
 */
static uint8_t v4_tuning_value(const PedalV4Tuning *tuning, PedalV4TuningSetting setting) {
    switch (setting) {
    case PEDAL_V4_TUNING_THROTTLE_CURVE:
        return tuning->throttle_curve;
    case PEDAL_V4_TUNING_BRAKE_CURVE:
        return tuning->brake_curve;
    case PEDAL_V4_TUNING_CLUTCH_CURVE:
        return tuning->clutch_curve;
    case PEDAL_V4_TUNING_BRAKE_FORCE:
        return tuning->brake_force;
    }
    return 0;
}

/**
 * @brief Selects the phase following one completed V4 operation.
 *
 * Preserves the observed tuning order and returns status or adjustment phases to request
 * selection.
 *
 * @param[in] phase Completed V4 phase.
 * @return Next V4 service phase.
 */
static PedalV4Phase next_v4_phase(PedalV4Phase phase) {
    switch (phase) {
    case PEDAL_V4_PHASE_BRAKE_FORCE:
        return PEDAL_V4_PHASE_CLUTCH_CURVE;
    case PEDAL_V4_PHASE_CLUTCH_CURVE:
        return PEDAL_V4_PHASE_BRAKE_CURVE;
    case PEDAL_V4_PHASE_BRAKE_CURVE:
        return PEDAL_V4_PHASE_THROTTLE_CURVE;
    case PEDAL_V4_PHASE_THROTTLE_CURVE:
        return PEDAL_V4_PHASE_SELECT;
    case PEDAL_V4_PHASE_STATUS:
        return PEDAL_V4_PHASE_SELECT;
    case PEDAL_V4_PHASE_SELECT:
        return PEDAL_V4_PHASE_SELECT;
    case PEDAL_V4_PHASE_ADJUSTMENT_START:
    case PEDAL_V4_PHASE_ADJUSTMENT_WAIT:
        return PEDAL_V4_PHASE_SELECT;
    case PEDAL_V4_PHASE_HOST_TRANSFER:
        return PEDAL_V4_PHASE_STATUS;
    }
    return PEDAL_V4_PHASE_SELECT;
}

/**
 * @brief Enters the asynchronous completion wait for a V4 pedal adjustment.
 *
 * Releases the initial request slot, starts the 20-second operation deadline, and schedules the
 * first empty-command heartbeat after 100 milliseconds.
 *
 * @param[in,out] service Pedal adjustment and transfer state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void begin_adjustment_wait(PedalService *service, uint32_t now_ms) {
    service->v4_phase = PEDAL_V4_PHASE_ADJUSTMENT_WAIT;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->v4_operation_deadline_ms = now_ms + PEDAL_V4_OPERATION_TIMEOUT_MS;
    service->next_v4_keepalive_ms = now_ms + PEDAL_V4_KEEPALIVE_INTERVAL_MS;
}

/**
 * @brief Finishes the active V4 pedal adjustment.
 *
 * Clears the pending source and returns request selection to the remaining tuning, adjustment, or
 * status work.
 *
 * @param[in,out] service Pedal adjustment and phase state to finish.
 */
static void finish_adjustment(PedalService *service) {
    PedalAdjustmentSource source = service->adjustment_source;
    if (service->adjustment_source == PEDAL_ADJUSTMENT_SOURCE_HOST) {
        service->host_adjustment_pending = false;
    } else if (service->adjustment_source == PEDAL_ADJUSTMENT_SOURCE_BUTTON) {
        service->button_adjustment_pending = false;
    }
    service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
    if (source == PEDAL_ADJUSTMENT_SOURCE_HOST && service->button_adjustment_pending) {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_BUTTON;
        service->v4_phase = PEDAL_V4_PHASE_ADJUSTMENT_START;
    } else {
        service->v4_phase = PEDAL_V4_PHASE_HOST_TRANSFER;
    }
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->v4_response_deadline_ms = 0;
    service->v4_operation_deadline_ms = 0;
    service->next_v4_keepalive_ms = 0;
}

/**
 * @brief Finishes the active generic V4 host request phase.
 *
 * Drops the front request after a timeout or leaves it active until the USB service releases its
 * forwarded response, then returns to the periodic status request.
 *
 * @param[in,out] service Pedal request queue and V4 phase state to finish.
 * @param[in] timed_out True when the request completed without a response.
 */
static void finish_host_transfer(PedalService *service, bool timed_out) {
    if (timed_out) {
        pedal_transfer_queue_finish(&service->host_transfer_queue);
    }
    service->v4_phase = PEDAL_V4_PHASE_STATUS;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->v4_response_deadline_ms = 0;
}

static void select_post_tuning_phase(PedalService *service) {
    if (service->host_adjustment_pending) {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_HOST;
        service->v4_phase = PEDAL_V4_PHASE_ADJUSTMENT_START;
    } else if (service->button_adjustment_pending) {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_BUTTON;
        service->v4_phase = PEDAL_V4_PHASE_ADJUSTMENT_START;
    } else {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
        service->v4_phase = PEDAL_V4_PHASE_HOST_TRANSFER;
    }
}

/**
 * @brief Completes the current V4 response phase.
 *
 * Moves an adjustment from its initial response into the asynchronous wait, finishes a final
 * adjustment response, or acknowledges the tuning value that was actually sent.
 *
 * @param[in,out] service V4 phase and pending-setting state to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void complete_v4_response(PedalService *service, uint32_t now_ms) {
    if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_START) {
        begin_adjustment_wait(service, now_ms);
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_WAIT) {
        finish_adjustment(service);
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_HOST_TRANSFER) {
        finish_host_transfer(service, false);
        return;
    }
    PedalV4TuningSetting setting = v4_phase_setting(service->v4_phase);
    if (setting != 0 && v4_tuning_value(&service->v4_tuning, setting) == service->v4_sent_value) {
        uint8_t mask = v4_setting_mask(setting);
        service->v4_tuning_pending = (uint8_t)(service->v4_tuning_pending & (uint8_t)~mask);
    }
    if (service->v4_phase == PEDAL_V4_PHASE_THROTTLE_CURVE) {
        select_post_tuning_phase(service);
    } else {
        service->v4_phase = next_v4_phase(service->v4_phase);
    }
    service->v4_response_received = false;
    service->v4_request_active = false;
}

/**
 * @brief Selects the highest-priority pending V4 pedal operation.
 *
 * Applies tuning in brake-force, clutch, throttle, and brake order, then selects host and
 * wheel-button adjustments and queued host transfers before the periodic status request.
 *
 * @param[in,out] service Pending V4 work and selected phase.
 */
static void select_v4_phase(PedalService *service) {
    uint8_t pending = service->v4_tuning_pending;
    if ((pending & v4_setting_mask(PEDAL_V4_TUNING_BRAKE_FORCE)) != 0) {
        service->v4_phase = PEDAL_V4_PHASE_BRAKE_FORCE;
    } else if ((pending & v4_setting_mask(PEDAL_V4_TUNING_CLUTCH_CURVE)) != 0) {
        service->v4_phase = PEDAL_V4_PHASE_CLUTCH_CURVE;
    } else if ((pending & v4_setting_mask(PEDAL_V4_TUNING_THROTTLE_CURVE)) != 0) {
        service->v4_phase = PEDAL_V4_PHASE_THROTTLE_CURVE;
    } else if ((pending & v4_setting_mask(PEDAL_V4_TUNING_BRAKE_CURVE)) != 0) {
        service->v4_phase = PEDAL_V4_PHASE_BRAKE_CURVE;
    } else if (service->host_adjustment_pending) {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_HOST;
        service->v4_phase = PEDAL_V4_PHASE_ADJUSTMENT_START;
    } else if (service->button_adjustment_pending) {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_BUTTON;
        service->v4_phase = PEDAL_V4_PHASE_ADJUSTMENT_START;
    } else if (pedal_transfer_queue_front(&service->host_transfer_queue) != NULL) {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
        service->v4_phase = PEDAL_V4_PHASE_HOST_TRANSFER;
    } else {
        service->adjustment_source = PEDAL_ADJUSTMENT_SOURCE_NONE;
        service->v4_phase = PEDAL_V4_PHASE_STATUS;
    }
}

/**
 * @brief Submits the request selected by the current V4 service phase.
 *
 * Selects pending tuning and adjustment work and emits periodic status requests when their
 * 15-millisecond deadline is reached. Requests are ignored unless the service is in an established
 * V4 stream. Adjustment requests wait up to 100 milliseconds for their initial response before
 * advancing to the asynchronous operation phase.
 *
 * @param[in,out] service V4 phase, request, tuning, and polling state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void send_v4_request(PedalService *service, uint32_t now_ms) {
    if (service->phase != PEDAL_SERVICE_V4_STREAM || !service->v4.active) {
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_SELECT) {
        select_v4_phase(service);
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_STATUS) {
        if (platform_time_reached(now_ms, service->next_status_ms) &&
            transfer_session_send(&service->v4, pedal_v4_status_request(),
                                  PEDAL_V4_STATUS_REQUEST_SIZE, 0)) {
            service->v4_request_active = true;
            service->next_status_ms = now_ms + PEDAL_V4_STATUS_INTERVAL_MS;
        }
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_START) {
        if (transfer_session_send(&service->v4, pedal_adjustment_probe_request(),
                                  PEDAL_ADJUSTMENT_PROBE_REQUEST_SIZE, 0)) {
            service->v4_request_active = true;
            service->v4_response_deadline_ms = now_ms + PEDAL_V4_RESPONSE_TIMEOUT_MS;
            service->adjustment_display = PEDAL_ADJUSTMENT_DISPLAY_HOLD;
            service->adjustment_display_pending = true;
        }
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_HOST_TRANSFER) {
        const PedalTransferRequest *request =
            pedal_transfer_queue_front(&service->host_transfer_queue);
        if (request == NULL) {
            service->v4_phase = PEDAL_V4_PHASE_STATUS;
        } else if (transfer_session_send(&service->v4, request->data, request->length, 0)) {
            pedal_transfer_queue_start(&service->host_transfer_queue);
            service->v4_request_active = true;
            service->v4_response_deadline_ms = now_ms + PEDAL_V4_RESPONSE_TIMEOUT_MS;
        }
        return;
    }

    PedalV4TuningSetting setting = v4_phase_setting(service->v4_phase);
    if (setting == 0) {
        return;
    }
    uint8_t mask = v4_setting_mask(setting);
    if ((service->v4_tuning_pending & mask) == 0) {
        if (service->v4_phase == PEDAL_V4_PHASE_THROTTLE_CURVE) {
            select_post_tuning_phase(service);
        } else {
            service->v4_phase = next_v4_phase(service->v4_phase);
        }
        return;
    }

    uint8_t value = v4_tuning_value(&service->v4_tuning, setting);
    if (pedal_v4_tuning_request(setting, value, service->v4_tuning_request) &&
        transfer_session_send(&service->v4, service->v4_tuning_request,
                              (uint8_t)PEDAL_V4_TUNING_REQUEST_SIZE, 0)) {
        service->v4_sent_value = value;
        service->v4_request_active = true;
    }
}

/**
 * @brief Services V4 requests and status polls in their protocol order.
 *
 * Receives transfer frames, completes accepted responses, maintains adjustment heartbeats and
 * deadlines, and selects adjustment, queued host, tuning, or periodic status work whenever the
 * current request slot becomes idle. Defers a timed-out session so the reconnect hold deadline is
 * anchored before link setup on the following service passes.
 *
 * @param[in,out] service Pedal state, transfer session, and published axes to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v4_stream(PedalService *service, uint32_t now_ms) {
    uint16_t length = platform_pedal_link_take_transfer(service->transfer_buffer,
                                                        sizeof(service->transfer_buffer));
    if (length != 0) {
        (void)transfer_session_receive(&service->v4, service->transfer_buffer, length);
    }

    if (transfer_session_poll(&service->v4) != TRANSFER_SESSION_OK) {
        service->digital_activity = true;
        service->phase = PEDAL_SERVICE_RECONNECT_HOLD_START;
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_WAIT) {
        if (time_passed(now_ms, service->next_v4_keepalive_ms)) {
            (void)transfer_session_keepalive(&service->v4);
            service->next_v4_keepalive_ms = now_ms + PEDAL_V4_KEEPALIVE_INTERVAL_MS;
        }
        if (service->v4_response_received) {
            complete_v4_response(service, now_ms);
        } else if (platform_time_reached(now_ms, service->v4_operation_deadline_ms)) {
            finish_adjustment(service);
        }
        return;
    }
    if (service->v4_response_received) {
        complete_v4_response(service, now_ms);
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_ADJUSTMENT_START && service->v4_request_active &&
        platform_time_reached(now_ms, service->v4_response_deadline_ms)) {
        begin_adjustment_wait(service, now_ms);
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_HOST_TRANSFER && service->v4_request_active &&
        platform_time_reached(now_ms, service->v4_response_deadline_ms)) {
        finish_host_transfer(service, true);
        return;
    }
    if (service->v4_request_active) {
        return;
    }
    send_v4_request(service, now_ms);
}

/**
 * @brief Advances legacy polling to the next channel request.
 *
 * Wraps the four-channel sequence from auxiliary input back to the first pedal axis.
 *
 * @param[in,out] service Legacy channel and service phase to advance.
 */
static void advance_legacy_channel(PedalService *service) {
    service->legacy_channel =
        (PedalLegacyChannel)((service->legacy_channel + 1) % PEDAL_LEGACY_CHANNEL_COUNT);
    service->phase = PEDAL_SERVICE_LEGACY_REQUEST;
}

/**
 * @brief Applies a legacy response or advances its bounded retry sequence.
 *
 * Publishes each received channel byte, records first-axis link activity, and advances to the
 * next request.
 * A channel that exhausts its retry allowance is released before digital reconnection begins.
 *
 * @param[in,out] service Legacy input, retry, activity, and reconnect state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_legacy_response(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_byte(&service->response)) {
        pedal_legacy_apply_response(service->legacy_channel, service->response, false,
                                    &service->input);
        if (service->legacy_channel == PEDAL_LEGACY_AUXILIARY) {
            service->remote_auxiliary = service->input.auxiliary;
            publish_auxiliary(service);
        }
        service->legacy_retries[service->legacy_channel] = 0;
        if (service->legacy_channel == PEDAL_LEGACY_AXIS_1) {
            service->digital_activity = true;
        }
        if (service->legacy_channel == PEDAL_LEGACY_AUXILIARY) {
            service->connected = true;
        }
        advance_legacy_channel(service);
        return;
    }
    if (!platform_time_reached(now_ms, service->deadline_ms)) {
        return;
    }

    uint8_t limit = service->legacy_channel == PEDAL_LEGACY_AXIS_1 ? PEDAL_LEGACY_AXIS_1_RETRY_LIMIT
                                                                   : PEDAL_LEGACY_RETRY_LIMIT;
    uint8_t *retries = &service->legacy_retries[service->legacy_channel];
    if (*retries < limit) {
        (*retries)++;
        advance_legacy_channel(service);
        return;
    }

    if (service->legacy_channel < PEDAL_LEGACY_AUXILIARY) {
        service->input.axes[service->legacy_channel] = 0;
    } else {
        service->remote_auxiliary = 0;
        publish_auxiliary(service);
    }
    *retries = 0;
    reconnect(service, now_ms);
}

/**
 * @brief Restarts the long V3 startup response window.
 *
 * Clears the accepted-frame count, sets the extended-status handshake latch, and restores the
 * initial fifteen-second report deadline.
 *
 * @param[in,out] service V3 startup counter and report deadline to reset.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void restart_v3_timeout(PedalService *service, uint32_t now_ms) {
    service->startup_frame_count = 0;
    service->status_handshake_active = true;
    service->deadline_ms = now_ms + PEDAL_INITIAL_SAMPLE_TIMEOUT_MS;
}

/**
 * @brief Sends the pending V3 calibration configuration report.
 *
 * Builds the current precision selection and reset marker, then clears the retained request only
 * after the pedal link accepts the frame.
 *
 * @param[in,out] service V3 service and pending configuration state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the configuration report was accepted by the pedal link.
 */
bool pedal_service_flush_configuration(PedalService *service, uint32_t now_ms) {
    if (service == NULL || service->phase != PEDAL_SERVICE_V3_STREAM ||
        !service->configuration_pending) {
        return false;
    }

    bool fine_scale = (service->v3.primary_calibration && !service->v3.legacy_calibration) ||
                      service->v3.secondary_calibration;
    pedal_v3_build_configuration(service->configuration_brake_force, fine_scale,
                                 service->configuration_reset_pending, &service->transmit_frame);
    if (!send_frame(service)) {
        return false;
    }
    if (service->configuration_reset_pending) {
        restart_v3_timeout(service, now_ms);
    }
    service->configuration_pending = false;
    service->configuration_reset_pending = false;
    return true;
}

/**
 * @brief Services one V3 status-report phase.
 *
 * Advances to the control phase after one status attempt, including a suppressed or busy attempt.
 *
 * @param[in,out] service V3 status and phase state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_status(PedalService *service, uint32_t now_ms) {
    const PedalProtocolStatus *status = &service->protocol_status;
    const PedalProtocolStatus *transmitted = &service->transmitted_status;
    bool status_changed =
        status->value != transmitted->value || status->first != transmitted->first ||
        status->second != transmitted->second || status->scale != transmitted->scale;
    bool status_due =
        !service->status_transmitted || status_changed || now_ms > service->next_status_ms;
    if (!(service->v3.primary_calibration && service->v3.secondary_calibration) && status_due) {
        pedal_v3_build_status(status, &service->transmit_frame);
        if (send_frame(service)) {
            service->transmitted_status = service->protocol_status;
            service->status_transmitted = true;
            service->next_status_ms = now_ms + PEDAL_STATUS_INTERVAL_MS;
        }
    }
    service->v3_phase = PEDAL_V3_PHASE_CONTROL;
}

/**
 * @brief Services one V3 control-report phase.
 *
 * Advances to the input phase after one control attempt, including when no direct control is
 * pending.
 *
 * @param[in,out] service V3 control and phase state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_control(PedalService *service, uint32_t now_ms) {
    if (service->pending_control != 0) {
        if ((service->pending_control & 0x1fu) != 0) {
            uint8_t remaining_control =
                pedal_v3_build_control(service->pending_control, &service->transmit_frame);
            uint8_t sent_control = service->pending_control ^ remaining_control;
            if (send_frame(service)) {
                service->pending_control = remaining_control;
                if ((sent_control & PEDAL_V3_CONTROL_ENABLE) != 0) {
                    service->v3.connection_flags = UINT8_MAX;
                }
                if ((sent_control & PEDAL_V3_CONTROL_DISABLE) != 0) {
                    service->v3.connection_flags = 0;
                }
                for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
                    service->input_command[axis] = 0;
                }
                service->input_command_pending = true;
                restart_v3_timeout(service, now_ms);
            }
        }
    }
    service->v3_phase = PEDAL_V3_PHASE_INPUT;
}

/**
 * @brief Services one V3 input-report phase.
 *
 * Queues periodic zero input after its deadline, sends one pending command, and advances to the
 * calibration phases or the next receive pass.
 *
 * @param[in,out] service V3 input and phase state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_input(PedalService *service, uint32_t now_ms) {
    if (!service->input_command_pending && now_ms > service->next_input_command_ms) {
        for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
            service->input_command[axis] = 0;
        }
        service->input_command_pending = true;
        service->next_input_command_ms = now_ms + PEDAL_INPUT_COMMAND_INTERVAL_MS;
    }
    if (service->input_command_pending) {
        pedal_v3_build_input_command(service->input_command, &service->transmit_frame);
        if (send_frame(service)) {
            if (service->input_command[0] != 0) {
                restart_v3_timeout(service, now_ms);
            }
            service->input_command_pending = false;
        }
    }
    service->v3_phase = service->v3.primary_calibration || service->v3.secondary_calibration
                            ? PEDAL_V3_PHASE_CONFIGURATION
                            : PEDAL_V3_PHASE_SAMPLE;
}

/**
 * @brief Services one V3 configuration-report phase.
 *
 * Sends one pending calibration configuration and advances to the keepalive phase.
 *
 * @param[in,out] service V3 configuration and phase state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_configuration(PedalService *service, uint32_t now_ms) {
    bool calibrating = service->v3.primary_calibration || service->v3.secondary_calibration;
    if (calibrating && service->configuration_pending) {
        bool fine_scale = (service->v3.primary_calibration && !service->v3.legacy_calibration) ||
                          service->v3.secondary_calibration;
        pedal_v3_build_configuration(service->configuration_brake_force, fine_scale,
                                     service->configuration_reset_pending,
                                     &service->transmit_frame);
        if (send_frame(service)) {
            if (service->configuration_reset_pending) {
                restart_v3_timeout(service, now_ms);
            }
            service->configuration_pending = false;
            service->configuration_reset_pending = false;
        }
    }
    service->v3_phase = PEDAL_V3_PHASE_KEEPALIVE;
}

/**
 * @brief Services one V3 calibration keepalive phase.
 *
 * Sends a due keepalive and advances to the next receive pass.
 *
 * @param[in,out] service V3 keepalive and phase state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_keepalive(PedalService *service, uint32_t now_ms) {
    bool calibrating = service->v3.primary_calibration || service->v3.secondary_calibration;
    if (calibrating && now_ms > service->next_keepalive_ms) {
        pedal_v3_build_keepalive(&service->transmit_frame);
        if (send_frame(service)) {
            service->next_keepalive_ms = now_ms + PEDAL_KEEPALIVE_INTERVAL_MS;
        }
    }
    service->v3_phase = PEDAL_V3_PHASE_SAMPLE;
}

/**
 * @brief Dispatches one V3 outbound report phase.
 *
 * Sample processing remains in service_v3_sample(), so each service pass performs at most one
 * receive or outbound operation from the official sample, status, control, input, configuration,
 * keepalive sequence.
 *
 * @param[in,out] service V3 report phase state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_output(PedalService *service, uint32_t now_ms) {
    switch (service->v3_phase) {
    case PEDAL_V3_PHASE_STATUS:
        service_v3_status(service, now_ms);
        break;
    case PEDAL_V3_PHASE_CONTROL:
        service_v3_control(service, now_ms);
        break;
    case PEDAL_V3_PHASE_INPUT:
        service_v3_input(service, now_ms);
        break;
    case PEDAL_V3_PHASE_CONFIGURATION:
        service_v3_configuration(service, now_ms);
        break;
    case PEDAL_V3_PHASE_KEEPALIVE:
        service_v3_keepalive(service, now_ms);
        break;
    default:
        break;
    }
}

/**
 * @brief Services one V3 sample-receive phase.
 *
 * Refreshes the report deadline for every structurally valid frame, including unknown report types.
 * Applies recognized reports, clears the extended-status handshake latch after 250 recognized
 * startup frames, and reconnects when the report deadline expires. Each receive pass advances to
 * the status phase; outbound phases advance independently on later service passes.
 * The auxiliary source lock is passed to V3 decoding while the remote auxiliary byte is retained
 * independently for later release of a local override.
 *
 * @param[in,out] service V3 transport, input, activity, and timeout state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_v3_sample(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_frame(service->frame_buffer) &&
        pedal_frame_decode(service->frame_buffer, &service->receive_frame) == PEDAL_FRAME_VALID) {
        bool startup_timeout = service->startup_frame_count < PEDAL_STARTUP_FRAME_COUNT;
        service->deadline_ms =
            now_ms + (startup_timeout ? PEDAL_INITIAL_SAMPLE_TIMEOUT_MS : PEDAL_SAMPLE_TIMEOUT_MS);
        if (pedal_v3_apply_report(&service->receive_frame, service->auxiliary_override_active,
                                  &service->v3, &service->input)) {
            if (service->receive_frame.type == PEDAL_V3_BRAKE_FORCE_REPORT) {
                service->alternate_brake_force_received = true;
            }
            if (service->receive_frame.type == PEDAL_FRAME_AXIS_SAMPLE) {
                service->remote_auxiliary = service->receive_frame.payload[7];
                publish_auxiliary(service);
            }
            if (service->startup_frame_count < PEDAL_STARTUP_FRAME_COUNT) {
                service->startup_frame_count++;
            } else {
                service->startup_handshake_active = false;
                service->status_handshake_active = false;
            }
            service->input.axes[1] =
                service->v3.primary_calibration || service->v3.secondary_calibration
                    ? service->v3.raw_brake
                    : pedal_input_scale_brake(service->v3.raw_brake, service->brake_force_percent);
            service->connected = true;
            service->digital_activity = true;
        }
    }

    if (platform_time_reached(now_ms, service->deadline_ms)) {
        service->recovery_handshake = true;
        if (!service->digital_activity) {
            pedal_v3_build_handshake(true, &service->transmit_frame);
            if (send_frame(service)) {
                service->recovery_handshake = false;
            }
        }
        reconnect(service, now_ms);
        return;
    }

    service->v3_phase = PEDAL_V3_PHASE_STATUS;
}

/**
 * @brief Advances the pedal transport service at its one-millisecond cadence.
 *
 * Processes at most one discovery, legacy, V3, V4, reconnect, or analog phase per elapsed
 * millisecond. V3 stream processing alternates one receive or outbound report phase per pass.
 * Calls made before the next service deadline leave the retained phase unchanged.
 *
 * @param[in,out] service Pedal transport, protocol, timing, and input state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void pedal_service_run(PedalService *service, uint32_t now_ms) {
    service->clock_ms = now_ms;
    if (!platform_time_reached(now_ms, service->next_service_ms)) {
        return;
    }
    service->next_service_ms = now_ms + 1;
    switch (service->phase) {
    case PEDAL_SERVICE_DETECT_REQUEST:
        if (platform_pedal_link_send_byte(PEDAL_DETECT_COMMAND)) {
            service->phase = PEDAL_SERVICE_DETECT_RESPONSE;
            service->deadline_ms = now_ms + PEDAL_DISCOVERY_TIMEOUT_MS;
        }
        break;
    case PEDAL_SERVICE_DETECT_RESPONSE:
        service_detect_response(service, now_ms);
        break;
    case PEDAL_SERVICE_PROTOCOL_REQUEST: {
        uint8_t command = service->device == PEDAL_DEVICE_V4 ? PEDAL_V4_PROTOCOL_COMMAND
                                                             : PEDAL_V3_PROTOCOL_COMMAND;
        if (platform_pedal_link_send_byte(command)) {
            service->phase = PEDAL_SERVICE_PROTOCOL_RESPONSE;
            service->deadline_ms = now_ms + PEDAL_PROTOCOL_TIMEOUT_MS;
        }
        break;
    }
    case PEDAL_SERVICE_PROTOCOL_RESPONSE:
        service_protocol_response(service, now_ms);
        break;
    case PEDAL_SERVICE_SELECT_PROTOCOL:
        service_select_protocol(service, now_ms);
        break;
    case PEDAL_SERVICE_LEGACY_REQUEST:
        if (platform_pedal_link_send_byte(pedal_legacy_request(service->legacy_channel,
                                                               service->protocol_status.first,
                                                               service->protocol_status.second))) {
            service->phase = PEDAL_SERVICE_LEGACY_RESPONSE;
            service->deadline_ms = now_ms + PEDAL_LEGACY_RESPONSE_TIMEOUT_MS;
        }
        break;
    case PEDAL_SERVICE_LEGACY_RESPONSE:
        service_legacy_response(service, now_ms);
        break;
    case PEDAL_SERVICE_V3_SWITCH_WAIT:
        if (platform_time_reached(now_ms, service->deadline_ms)) {
            platform_pedal_link_begin_framed_receive();
            service->phase = PEDAL_SERVICE_V3_START;
        }
        break;
    case PEDAL_SERVICE_V3_START:
        pedal_v3_build_handshake(service->recovery_handshake, &service->transmit_frame);
        if (send_frame(service)) {
            service->startup_frame_count = 0;
            service->startup_handshake_active = true;
            service->status_handshake_active = true;
            service->phase = PEDAL_SERVICE_V3_STREAM;
            service->deadline_ms = now_ms + PEDAL_INITIAL_SAMPLE_TIMEOUT_MS;
            service->recovery_handshake = false;
            service->status_transmitted = false;
            service->next_input_command_ms = 0;
            service->next_keepalive_ms = 0;
            service->v3_phase = PEDAL_V3_PHASE_SAMPLE;
        }
        break;
    case PEDAL_SERVICE_V3_STREAM:
        if (service->v3_phase == PEDAL_V3_PHASE_SAMPLE) {
            service_v3_sample(service, now_ms);
        } else {
            service_v3_output(service, now_ms);
        }
        break;
    case PEDAL_SERVICE_V4_START:
        platform_pedal_link_begin_transfer_receive();
        if (transfer_session_init(&service->v4, &v4_callbacks, service)) {
            service->phase = PEDAL_SERVICE_V4_STREAM;
            service->next_status_ms = 0;
            service->v4_phase = PEDAL_V4_PHASE_STATUS;
            service->v4_request_active = false;
            service->v4_response_received = false;
        } else {
            reconnect(service, now_ms);
        }
        break;
    case PEDAL_SERVICE_V4_STREAM:
        service_v4_stream(service, now_ms);
        break;
    case PEDAL_SERVICE_RECONNECT_HOLD_START:
        service->deadline_ms = now_ms + PEDAL_RECONNECT_DELAY_MS;
        service->phase = PEDAL_SERVICE_RECONNECT_WAIT;
        break;
    case PEDAL_SERVICE_RECONNECT_WAIT:
        if (service->digital_activity) {
            setup_reconnect_link(service);
        } else {
            platform_pedal_link_stop_receive();
        }
        if (platform_time_reached(now_ms, service->deadline_ms)) {
            release_published_input(service);
            platform_pedal_link_begin_discovery();
            service->phase = PEDAL_SERVICE_DETECT_REQUEST;
        }
        break;
    case PEDAL_SERVICE_ANALOG:
        break;
    }
}

/**
 * @brief Returns the pedal input currently published by the active source.
 *
 * Exposes the service-owned input state without copying it.
 *
 * @param[in] service Pedal service containing the published axes and auxiliary input.
 * @return Read-only current pedal input.
 */
const PedalInput *pedal_service_input(const PedalService *service) { return &service->input; }

/**
 * @brief Returns the retained V3 pedal protocol state.
 *
 * Exposes the service-owned V3 report state without copying it.
 *
 * @param[in] service Pedal service containing V3 calibration and connection state.
 * @return Read-only current V3 state.
 */
const PedalV3State *pedal_service_v3_state(const PedalService *service) { return &service->v3; }
