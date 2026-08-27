#include "pedal/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"
#include "platform/pedal_link.h"
#include "platform/time.h"

enum {
    PEDAL_DETECT_COMMAND = 0x0a,
    PEDAL_V3_PROTOCOL_COMMAND = 0x05,
    PEDAL_V4_PROTOCOL_COMMAND = 0x06,
    PEDAL_V3_PROTOCOL_RESPONSE = 0x15,
    PEDAL_V4_PROTOCOL_RESPONSE = 0x26,
    PEDAL_HANDSHAKE_FRAME = 2,
    PEDAL_STATUS_FRAME = 0,
    PEDAL_DISCOVERY_TIMEOUT_MS = 100,
    PEDAL_INITIAL_SAMPLE_TIMEOUT_MS = 15000,
    PEDAL_SAMPLE_TIMEOUT_MS = 1000,
    PEDAL_RECONNECT_DELAY_MS = 550,
    PEDAL_STATUS_INTERVAL_MS = 500,
};

static void reconnect(PedalService *service, uint32_t now_ms) {
    pedal_input_release(&service->input);
    service->connected = false;
    service->device = PEDAL_DEVICE_NONE;
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

static bool send_frame(PedalService *service, uint8_t type, uint8_t first, uint8_t second) {
    service->transmit_frame = (PedalFrame){
        .type = type,
        .payload = {first, second, 0, 0, 0, 0, 0, 0},
    };
    pedal_frame_encode(&service->transmit_frame, service->frame_buffer);
    return platform_pedal_link_send_frame(service->frame_buffer);
}

void pedal_service_init(PedalService *service) {
    pedal_input_release(&service->input);
    pedal_analog_init(&service->analog);
    service->phase = PEDAL_SERVICE_DETECT_REQUEST;
    service->device = PEDAL_DEVICE_NONE;
    service->deadline_ms = 0;
    service->next_status_ms = 0;
    service->analog_samples_ready = false;
    service->connected = false;
}

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
        if (service->device == PEDAL_DEVICE_V3 && service->response == PEDAL_V3_PROTOCOL_RESPONSE) {
            platform_pedal_link_begin_framed_receive();
            service->phase = PEDAL_SERVICE_V3_START;
            return;
        }
        if (service->device == PEDAL_DEVICE_V4 && service->response == PEDAL_V4_PROTOCOL_RESPONSE) {
            service->phase = PEDAL_SERVICE_V4_UNSUPPORTED;
            return;
        }
    }
    if (platform_time_reached(now_ms, service->deadline_ms)) {
        reconnect(service, now_ms);
    }
}

static void service_v3_stream(PedalService *service, uint32_t now_ms) {
    if (platform_pedal_link_take_frame(service->frame_buffer) &&
        pedal_frame_decode(service->frame_buffer, &service->receive_frame) == PEDAL_FRAME_VALID) {
        service->deadline_ms = now_ms + PEDAL_SAMPLE_TIMEOUT_MS;
        if (pedal_input_decode(&service->receive_frame, &service->input)) {
            service->connected = true;
        }
    }

    if (platform_time_reached(now_ms, service->deadline_ms)) {
        reconnect(service, now_ms);
        return;
    }
    if (platform_time_reached(now_ms, service->next_status_ms) &&
        send_frame(service, PEDAL_STATUS_FRAME, 0, 0)) {
        service->next_status_ms = now_ms + PEDAL_STATUS_INTERVAL_MS;
    }
}

void pedal_service_run(PedalService *service, uint32_t now_ms) {
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
    case PEDAL_SERVICE_V3_START:
        if (send_frame(service, PEDAL_HANDSHAKE_FRAME, 0xff, 0)) {
            service->phase = PEDAL_SERVICE_V3_STREAM;
            service->deadline_ms = now_ms + PEDAL_INITIAL_SAMPLE_TIMEOUT_MS;
            service->next_status_ms = now_ms + PEDAL_STATUS_INTERVAL_MS;
        }
        break;
    case PEDAL_SERVICE_V3_STREAM:
        service_v3_stream(service, now_ms);
        break;
    case PEDAL_SERVICE_RECONNECT_WAIT:
        if (platform_time_reached(now_ms, service->deadline_ms)) {
            service->phase = PEDAL_SERVICE_DETECT_REQUEST;
        }
        break;
    case PEDAL_SERVICE_ANALOG:
        break;
    case PEDAL_SERVICE_V4_UNSUPPORTED:
        break;
    }
}

const PedalInput *pedal_service_input(const PedalService *service) { return &service->input; }
