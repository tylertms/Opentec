#include "pedal/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"
#include "pedal/protocol.h"
#include "pedal/v4_status.h"
#include "pedal/v4_tuning.h"
#include "platform/pedal_link.h"
#include "platform/time.h"
#include "transfer/session.h"

enum {
    PEDAL_DETECT_COMMAND = 0x0a,
    PEDAL_V3_PROTOCOL_COMMAND = 0x05,
    PEDAL_V4_PROTOCOL_COMMAND = 0x06,
    PEDAL_DISCOVERY_TIMEOUT_MS = 100,
    PEDAL_V3_BAUD_SWITCH_DELAY_MS = 5,
    PEDAL_INITIAL_SAMPLE_TIMEOUT_MS = 15000,
    PEDAL_SAMPLE_TIMEOUT_MS = 1000,
    PEDAL_STARTUP_FRAME_COUNT = 250,
    PEDAL_RECONNECT_DELAY_MS = 550,
    PEDAL_STATUS_INTERVAL_MS = 500,
    PEDAL_INPUT_COMMAND_INTERVAL_MS = 500,
    PEDAL_KEEPALIVE_INTERVAL_MS = 2500,
    PEDAL_V4_STATUS_INTERVAL_MS = 15,
    PEDAL_LEGACY_RESPONSE_TIMEOUT_MS = 17,
    PEDAL_LEGACY_AXIS_1_RETRY_LIMIT = 5,
    PEDAL_LEGACY_RETRY_LIMIT = 6,
    PEDAL_PROTOCOL_PRESERVE_VALUE = 0x66,
};

static const uint8_t pedal_v4_status_request[] = {
    0x12, 0x0a, 0x00, 0x00, 0x02, 0x08, 0x00, 0x00, 0x02, 0x18, 0x00,
    0x00, 0x01, 0x20, 0x00, 0x00, 0x08, 0xaa, 0x00, 0x00, 0x01,
};

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

static void clear_v3_outbound(PedalService *service) {
    service->pending_control = 0;
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        service->input_command[axis] = 0;
    }
    service->input_command_pending = false;
    service->configuration_pending = false;
    service->configuration_reset_pending = false;
    service->next_input_command_ms = 0;
    service->next_keepalive_ms = 0;
}

static void reconnect(PedalService *service, uint32_t now_ms) {
    pedal_input_release(&service->input);
    service->remote_auxiliary = 0;
    publish_auxiliary(service);
    pedal_v3_state_init(&service->v3);
    service->v4.active = false;
    clear_v3_outbound(service);
    service->connected = false;
    service->v4_phase = PEDAL_V4_PHASE_STATUS;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->device = PEDAL_DEVICE_NONE;
    service->startup_frame_count = 0;
    service->legacy_channel = PEDAL_LEGACY_AXIS_1;
    for (uint8_t channel = 0; channel < PEDAL_LEGACY_CHANNEL_COUNT; channel++) {
        service->legacy_retries[channel] = 0;
    }
    if (service->analog_samples_ready &&
        pedal_analog_update(&service->analog, service->analog_samples, &service->input)) {
        service->remote_auxiliary = service->input.auxiliary;
        publish_auxiliary(service);
        platform_pedal_link_begin_analog();
        service->connected = true;
        service->phase = PEDAL_SERVICE_ANALOG;
        return;
    }
    service->phase = PEDAL_SERVICE_RECONNECT_WAIT;
    service->deadline_ms = now_ms + PEDAL_RECONNECT_DELAY_MS;
    platform_pedal_link_begin_discovery();
}

static bool send_frame(PedalService *service) {
    pedal_frame_encode(&service->transmit_frame, service->frame_buffer);
    return platform_pedal_link_send_frame(service->frame_buffer);
}

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
    service->response = 0;
    service->brake_force_percent = 100;
    service->startup_frame_count = 0;
    service->configuration_brake_force = 0;
    service->v4_tuning_pending = 0;
    service->v4_sent_value = 0;
    service->remote_auxiliary = 0;
    service->auxiliary_override = 0;
    service->legacy_channel = PEDAL_LEGACY_AXIS_1;
    service->protocol_status = (PedalProtocolStatus){0};
    service->transmitted_status = (PedalProtocolStatus){0};
    service->v4_tuning = (PedalV4Tuning){0};
    service->v4_phase = PEDAL_V4_PHASE_STATUS;
    for (uint8_t channel = 0; channel < PEDAL_LEGACY_CHANNEL_COUNT; channel++) {
        service->legacy_retries[channel] = 0;
    }
    service->analog_samples_ready = false;
    service->connected = false;
    service->auxiliary_override_active = false;
    service->recovery_handshake = false;
    service->status_transmitted = false;
    service->v4_request_active = false;
    service->v4_response_received = false;
    service->clock_ms = 0;
    clear_v3_outbound(service);
}

static void send_v4_transfer(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    (void)platform_pedal_link_send_transfer(data, length);
}

static bool v4_transfer_busy(void *context) {
    (void)context;
    return platform_pedal_link_transmit_busy();
}

static void apply_v4_status(void *context, const uint8_t *data, uint8_t length, uint8_t group,
                            bool complete) {
    PedalService *service = context;
    (void)complete;
    if (group != 0 || !service->v4_request_active) {
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_STATUS) {
        pedal_v4_status_parse(data, length, service->input.axes);
    }
    service->connected = true;
    service->v4_response_received = true;
}

static uint32_t read_v4_clock(void *context) {
    const PedalService *service = context;
    return service->clock_ms;
}

static const TransferSessionCallbacks v4_callbacks = {
    .send = send_v4_transfer,
    .ready = v4_transfer_busy,
    .data = apply_v4_status,
    .clock = read_v4_clock,
};

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

void pedal_service_set_brake_force(PedalService *service, uint8_t force_percent) {
    service->brake_force_percent = force_percent;
}

static void update_v4_tuning_value(uint8_t *current, uint8_t value, uint8_t setting,
                                   uint8_t *pending) {
    if (*current != value) {
        *current = value;
        *pending |= (uint8_t)(1u << (setting - 1));
    }
}

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
 * Enables automatic settling while legacy pedal transport or either V3 calibration path is active
 * and no primary or secondary connection flag is asserted.
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
 * @brief Applies a host update to the pedal protocol status.
 *
 * Protocol updates always replace the first selector. Value 0x66 preserves the current value and
 * second selector; other values replace both. Legacy-scale updates are accepted only while the
 * byte-oriented legacy transport is active.
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

    bool legacy = service->phase == PEDAL_SERVICE_LEGACY_REQUEST ||
                  service->phase == PEDAL_SERVICE_LEGACY_RESPONSE;
    if (legacy) {
        service->protocol_status.scale = command->value;
    }
}

/**
 * @brief Reports whether the attached pedal path accepts calibration commands.
 *
 * Accepts commands during legacy transport or either active V3 calibration mode.
 *
 * @param[in] service Current pedal transport and calibration state.
 * @return True when pedal calibration commands may be queued.
 */
bool pedal_service_calibration_active(const PedalService *service) {
    bool legacy = service->phase == PEDAL_SERVICE_LEGACY_REQUEST ||
                  service->phase == PEDAL_SERVICE_LEGACY_RESPONSE;
    return legacy || service->v3.primary_calibration || service->v3.secondary_calibration;
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

void pedal_service_request_configuration(PedalService *service, uint8_t brake_force, bool reset) {
    service->configuration_brake_force = brake_force;
    service->configuration_pending = true;
    service->configuration_reset_pending |= reset;
}

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

static void service_protocol_response(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_byte(&service->response)) {
        service->phase = PEDAL_SERVICE_SELECT_PROTOCOL;
        return;
    }
    if (platform_time_reached(now_ms, service->deadline_ms)) {
        reconnect(service, now_ms);
    }
}

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

static uint8_t v4_setting_mask(PedalV4TuningSetting setting) {
    return (uint8_t)(1u << (setting - 1));
}

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
        return 0;
    }
    return 0;
}

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

static PedalV4Phase next_v4_phase(PedalV4Phase phase) {
    switch (phase) {
    case PEDAL_V4_PHASE_BRAKE_FORCE:
        return PEDAL_V4_PHASE_CLUTCH_CURVE;
    case PEDAL_V4_PHASE_CLUTCH_CURVE:
        return PEDAL_V4_PHASE_BRAKE_CURVE;
    case PEDAL_V4_PHASE_BRAKE_CURVE:
        return PEDAL_V4_PHASE_THROTTLE_CURVE;
    case PEDAL_V4_PHASE_THROTTLE_CURVE:
        return PEDAL_V4_PHASE_STATUS;
    case PEDAL_V4_PHASE_STATUS:
        return PEDAL_V4_PHASE_SELECT;
    case PEDAL_V4_PHASE_SELECT:
        return PEDAL_V4_PHASE_SELECT;
    }
    return PEDAL_V4_PHASE_SELECT;
}

static void complete_v4_response(PedalService *service) {
    PedalV4TuningSetting setting = v4_phase_setting(service->v4_phase);
    if (setting != 0 && v4_tuning_value(&service->v4_tuning, setting) == service->v4_sent_value) {
        uint8_t mask = v4_setting_mask(setting);
        service->v4_tuning_pending = (uint8_t)(service->v4_tuning_pending & (uint8_t)~mask);
    }
    service->v4_phase = next_v4_phase(service->v4_phase);
    service->v4_response_received = false;
    service->v4_request_active = false;
}

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
    } else {
        service->v4_phase = PEDAL_V4_PHASE_STATUS;
    }
}

static void send_v4_request(PedalService *service, uint32_t now_ms) {
    if (service->v4_phase == PEDAL_V4_PHASE_SELECT) {
        select_v4_phase(service);
        return;
    }
    if (service->v4_phase == PEDAL_V4_PHASE_STATUS) {
        if (now_ms > service->next_status_ms &&
            transfer_session_send(&service->v4, pedal_v4_status_request,
                                  (uint8_t)sizeof(pedal_v4_status_request), 0)) {
            service->v4_request_active = true;
            service->next_status_ms = now_ms + PEDAL_V4_STATUS_INTERVAL_MS;
        }
        return;
    }

    PedalV4TuningSetting setting = v4_phase_setting(service->v4_phase);
    if (setting == 0) {
        return;
    }
    uint8_t mask = v4_setting_mask(setting);
    if ((service->v4_tuning_pending & mask) == 0) {
        service->v4_phase = next_v4_phase(service->v4_phase);
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
 * @brief Services V4 tuning writes and status polls in their protocol order.
 * @param service Pedal state, transfer session, and published axes to update.
 * @param now_ms Current monotonic time in milliseconds.
 */
static void service_v4_stream(PedalService *service, uint32_t now_ms) {
    uint16_t length = platform_pedal_link_take_transfer(service->transfer_buffer,
                                                        sizeof(service->transfer_buffer));
    if (length != 0) {
        (void)transfer_session_receive(&service->v4, service->transfer_buffer, length);
    }

    if (transfer_session_poll(&service->v4) != TRANSFER_SESSION_OK) {
        reconnect(service, now_ms);
        return;
    }
    if (service->v4_response_received) {
        complete_v4_response(service);
        return;
    }
    if (service->v4_request_active) {
        return;
    }
    send_v4_request(service, now_ms);
}

static void advance_legacy_channel(PedalService *service) {
    service->legacy_channel =
        (PedalLegacyChannel)((service->legacy_channel + 1) % PEDAL_LEGACY_CHANNEL_COUNT);
    service->phase = PEDAL_SERVICE_LEGACY_REQUEST;
}

static void service_legacy_response(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_byte(&service->response)) {
        pedal_legacy_apply_response(service->legacy_channel, service->response, false,
                                    &service->input);
        if (service->legacy_channel == PEDAL_LEGACY_AUXILIARY) {
            service->remote_auxiliary = service->input.auxiliary;
            publish_auxiliary(service);
        }
        service->legacy_retries[service->legacy_channel] = 0;
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

static void restart_v3_timeout(PedalService *service, uint32_t now_ms) {
    service->startup_frame_count = 0;
    service->deadline_ms = now_ms + PEDAL_INITIAL_SAMPLE_TIMEOUT_MS;
}

static void service_v3_output(PedalService *service, uint32_t now_ms) {
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
        return;
    }

    if (service->pending_control != 0) {
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
        return;
    }

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
        return;
    }

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
        return;
    }
    if (calibrating && now_ms > service->next_keepalive_ms) {
        pedal_v3_build_keepalive(&service->transmit_frame);
        if (send_frame(service)) {
            service->next_keepalive_ms = now_ms + PEDAL_KEEPALIVE_INTERVAL_MS;
        }
    }
}

static void service_v3_stream(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_frame(service->frame_buffer) &&
        pedal_frame_decode(service->frame_buffer, &service->receive_frame) == PEDAL_FRAME_VALID) {
        if (pedal_v3_apply_report(&service->receive_frame, false, &service->v3, &service->input)) {
            if (service->receive_frame.type == PEDAL_FRAME_AXIS_SAMPLE) {
                service->remote_auxiliary = service->input.auxiliary;
                publish_auxiliary(service);
            }
            uint32_t timeout_ms = PEDAL_SAMPLE_TIMEOUT_MS;
            if (service->startup_frame_count < PEDAL_STARTUP_FRAME_COUNT) {
                service->startup_frame_count++;
                timeout_ms = PEDAL_INITIAL_SAMPLE_TIMEOUT_MS;
            }
            service->deadline_ms = now_ms + timeout_ms;
            service->input.axes[1] =
                service->v3.primary_calibration || service->v3.secondary_calibration
                    ? service->v3.raw_brake
                    : pedal_input_scale_brake(service->v3.raw_brake, service->brake_force_percent);
            service->connected = true;
        }
    }

    if (platform_time_reached(now_ms, service->deadline_ms)) {
        service->recovery_handshake = true;
        reconnect(service, now_ms);
        return;
    }

    service_v3_output(service, now_ms);
}

void pedal_service_run(PedalService *service, uint32_t now_ms) {
    service->clock_ms = now_ms;
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
            service->deadline_ms = now_ms + PEDAL_DISCOVERY_TIMEOUT_MS;
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
            service->phase = PEDAL_SERVICE_V3_STREAM;
            service->deadline_ms = now_ms + PEDAL_INITIAL_SAMPLE_TIMEOUT_MS;
            service->recovery_handshake = false;
            service->status_transmitted = false;
            service->next_input_command_ms = 0;
            service->next_keepalive_ms = 0;
        }
        break;
    case PEDAL_SERVICE_V3_STREAM:
        service_v3_stream(service, now_ms);
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
    case PEDAL_SERVICE_RECONNECT_WAIT:
        if (platform_time_reached(now_ms, service->deadline_ms)) {
            service->phase = PEDAL_SERVICE_DETECT_REQUEST;
        }
        break;
    case PEDAL_SERVICE_ANALOG:
        break;
    }
}

const PedalInput *pedal_service_input(const PedalService *service) { return &service->input; }

const PedalV3State *pedal_service_v3_state(const PedalService *service) { return &service->v3; }
