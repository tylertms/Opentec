#include "pedal/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"
#include "pedal/protocol.h"
#include "pedal/v4_status.h"
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
};

static const uint8_t pedal_v4_status_request[] = {
    0x12, 0x0a, 0x00, 0x00, 0x02, 0x08, 0x00, 0x00, 0x02, 0x18, 0x00,
    0x00, 0x01, 0x20, 0x00, 0x00, 0x08, 0xaa, 0x00, 0x00, 0x01,
};

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
    pedal_v3_state_init(&service->v3);
    service->v4.active = false;
    clear_v3_outbound(service);
    service->connected = false;
    service->v4_request_pending = false;
    service->v4_status_received = false;
    service->device = PEDAL_DEVICE_NONE;
    service->startup_frame_count = 0;
    service->legacy_channel = PEDAL_LEGACY_AXIS_1;
    for (uint8_t channel = 0; channel < PEDAL_LEGACY_CHANNEL_COUNT; channel++) {
        service->legacy_retries[channel] = 0;
    }
    if (service->analog_samples_ready && pedal_analog_detect(service->analog_samples[2])) {
        platform_pedal_link_begin_analog();
        pedal_analog_update(&service->analog, service->analog_samples, &service->input);
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
    service->legacy_channel = PEDAL_LEGACY_AXIS_1;
    service->protocol_status = (PedalProtocolStatus){0};
    service->transmitted_status = (PedalProtocolStatus){0};
    for (uint8_t channel = 0; channel < PEDAL_LEGACY_CHANNEL_COUNT; channel++) {
        service->legacy_retries[channel] = 0;
    }
    service->analog_samples_ready = false;
    service->connected = false;
    service->auxiliary_locked = false;
    service->recovery_handshake = false;
    service->status_transmitted = false;
    service->v4_request_pending = false;
    service->v4_status_received = false;
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
    if (group != 0 || !service->v4_request_pending) {
        return;
    }
    pedal_v4_status_parse(data, length, service->input.axes);
    service->connected = true;
    service->v4_request_pending = false;
    service->v4_status_received = true;
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
    if (service->phase == PEDAL_SERVICE_ANALOG) {
        pedal_analog_update(&service->analog, service->analog_samples, &service->input);
    }
}

void pedal_service_set_brake_force(PedalService *service, uint8_t force_percent) {
    service->brake_force_percent = force_percent > 100 ? 100 : force_percent;
}

void pedal_service_set_auxiliary_locked(PedalService *service, bool locked) {
    service->auxiliary_locked = locked;
}

void pedal_service_set_protocol_status(PedalService *service, const PedalProtocolStatus *status) {
    service->protocol_status = *status;
}

void pedal_service_request_control(PedalService *service, PedalV3Control control) {
    service->pending_control |= (uint8_t)control;
}

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

/**
 * @brief Services V4 transfers and polls status after each strict 15-millisecond deadline.
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
    if (service->v4_status_received) {
        service->v4_status_received = false;
        return;
    }
    if (!service->v4_request_pending && now_ms > service->next_status_ms &&
        transfer_session_send(&service->v4, pedal_v4_status_request,
                              (uint8_t)sizeof(pedal_v4_status_request), 0)) {
        service->v4_request_pending = true;
        service->next_status_ms = now_ms + PEDAL_V4_STATUS_INTERVAL_MS;
    }
}

static void advance_legacy_channel(PedalService *service) {
    service->legacy_channel =
        (PedalLegacyChannel)((service->legacy_channel + 1) % PEDAL_LEGACY_CHANNEL_COUNT);
    service->phase = PEDAL_SERVICE_LEGACY_REQUEST;
}

static void service_legacy_response(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_byte(&service->response)) {
        pedal_legacy_apply_response(service->legacy_channel, service->response,
                                    service->auxiliary_locked, &service->input);
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
        service->input.auxiliary = 0;
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
        if (pedal_v3_apply_report(&service->receive_frame, service->auxiliary_locked, &service->v3,
                                  &service->input)) {
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
            service->v4_request_pending = false;
            service->v4_status_received = false;
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
