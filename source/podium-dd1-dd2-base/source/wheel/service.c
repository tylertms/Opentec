#include "wheel/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "wheel/protocol.h"
#include "wheel/transport_service.h"

enum {
    WHEEL_PROTOCOL_TRANSPORT_COMMAND = 2,
    WHEEL_BUTTON_COMMAND = 3,
    WHEEL_BUTTON_PHASE_FIRST = 1,
    WHEEL_BUTTON_PHASE_SECOND = 2,
    WHEEL_BUTTON_PHASE_THIRD = 4,
    WHEEL_BUTTON_PHASE_AUXILIARY = 8,
    WHEEL_BUTTON_REQUEST_READY = 1,
    WHEEL_BUTTON_RESPONSE_READY = 2,
    WHEEL_BUTTON_PRIMARY_RESPONSE = 0xe0,
    WHEEL_BUTTON_SECONDARY_RESPONSE = 0xc0,
    WHEEL_BUTTON_RESPONSE_MASK = 0xe0,
    WHEEL_BUTTON_VALUE_MASK = 0x1f,
};

static void assign(uint8_t *value, uint8_t target, uint8_t source, uint8_t source_bit) {
    uint8_t mask = (uint8_t)(1u << target);
    *value = (*value & (uint8_t)~mask) | (((source >> source_bit) & 1u) << target);
}

static void apply_auxiliary(WheelService *service, uint8_t sample) {
    assign(&service->button_banks[0], 3, sample, 3);
    assign(&service->button_banks[0], 1, sample, 4);
    assign(&service->button_banks[0], 2, sample, 1);
    assign(&service->button_banks[0], 0, sample, 2);
    assign(&service->button_banks[2], 2, sample, 0);
}

static void apply_first(WheelService *service, uint8_t sample, bool secondary) {
    assign(&service->button_banks[2], 5, sample, 0);
    assign(&service->button_banks[2], 1, sample, 3);
    assign(&service->button_banks[1], 2, sample, 4);
    assign(&service->button_banks[1], 1, sample, 2);
    if (secondary) {
        assign(&service->button_banks[2], 3, sample, 1);
    }
}

static void apply_second(WheelService *service, uint8_t sample) {
    assign(&service->button_banks[1], 3, sample, 0);
    assign(&service->button_banks[1], 5, sample, 3);
    assign(&service->button_banks[1], 4, sample, 4);
    assign(&service->button_banks[1], 7, sample, 1);
    assign(&service->button_banks[1], 6, sample, 2);
}

static void apply_third(WheelService *service, uint8_t sample) {
    assign(&service->button_banks[0], 4, sample, 2);
    assign(&service->button_banks[0], 6, sample, 1);
    assign(&service->button_banks[0], 5, sample, 4);
    assign(&service->button_banks[0], 7, sample, 3);
    assign(&service->button_banks[1], 0, sample, 0);
}

static uint8_t expected_scan_response(const WheelService *service) {
    return service->protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY
               ? WHEEL_BUTTON_SECONDARY_RESPONSE
               : WHEEL_BUTTON_PRIMARY_RESPONSE;
}

static void apply_scan_response(WheelService *service, const WheelTransportFrame *response) {
    if (response == 0 || response->length != WHEEL_TRANSPORT_PAYLOAD_SIZE ||
        (response->data[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] & WHEEL_BUTTON_RESPONSE_READY) == 0) {
        return;
    }
    uint8_t encoded = response->data[1];
    uint8_t response_type = encoded & WHEEL_BUTTON_RESPONSE_MASK;
    if (response_type != expected_scan_response(service)) {
        return;
    }
    uint8_t sample = encoded & WHEEL_BUTTON_VALUE_MASK;
    switch (service->scan_phase) {
    case WHEEL_BUTTON_PHASE_FIRST:
        apply_first(service, sample, response_type == WHEEL_BUTTON_SECONDARY_RESPONSE);
        break;
    case WHEEL_BUTTON_PHASE_SECOND:
        apply_second(service, sample);
        break;
    case WHEEL_BUTTON_PHASE_THIRD:
        apply_third(service, sample);
        break;
    case WHEEL_BUTTON_PHASE_AUXILIARY:
        apply_auxiliary(service, sample);
        break;
    }
}

static void clear_buttons(WheelService *service) {
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        service->button_banks[bank] = 0;
    }
}

static void reset_connection(WheelService *service) {
    wheel_protocol_init(&service->protocol);
    clear_buttons(service);
    service->scan_phase = 0;
}

static void start_scan(WheelService *service, uint32_t now_ms) {
    service->scan_phase >>= 1;
    if (service->scan_phase == 0) {
        service->scan_phase = WHEEL_BUTTON_PHASE_AUXILIARY;
    }
    for (uint8_t index = 0; index < WHEEL_TRANSPORT_PAYLOAD_SIZE; index++) {
        service->request[index] = 0;
    }
    service->request[0] = service->scan_phase;
    service->request[1] = UINT8_MAX;
    service->request[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] = WHEEL_BUTTON_REQUEST_READY;
    service->request_kind = WHEEL_SERVICE_REQUEST_BUTTONS;
    if (!wheel_transport_service_start(&service->transport, WHEEL_BUTTON_COMMAND, service->request,
                                       sizeof(service->request), now_ms)) {
        service->transport.status = WHEEL_TRANSPORT_FAILED;
    }
}

static void start_protocol(WheelService *service, uint32_t now_ms) {
    service->request_kind = WHEEL_SERVICE_REQUEST_PROTOCOL;
    if (!wheel_transport_service_start(&service->transport, WHEEL_PROTOCOL_TRANSPORT_COMMAND,
                                       wheel_protocol_response(&service->protocol),
                                       WHEEL_PROTOCOL_PACKET_SIZE, now_ms)) {
        service->transport.status = WHEEL_TRANSPORT_FAILED;
    }
}

static bool scan_active(const WheelService *service) {
    return service->protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY ||
           service->protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY;
}

void wheel_service_init(WheelService *service) {
    wheel_transport_service_init(&service->transport);
    wheel_protocol_init(&service->protocol);
    clear_buttons(service);
    service->scan_phase = 0;
    service->request_kind = WHEEL_SERVICE_REQUEST_NONE;
}

void wheel_service_run(WheelService *service, uint32_t now_ms) {
    wheel_transport_service_run(&service->transport, now_ms);
    if (service->transport.status == WHEEL_TRANSPORT_PENDING) {
        return;
    }
    if (service->transport.status == WHEEL_TRANSPORT_SUCCEEDED) {
        const WheelTransportFrame *response = wheel_transport_service_response(&service->transport);
        if (service->request_kind == WHEEL_SERVICE_REQUEST_PROTOCOL && response != 0 &&
            response->length == WHEEL_PROTOCOL_PACKET_SIZE) {
            wheel_protocol_accept(&service->protocol, response->data);
        } else if (service->request_kind == WHEEL_SERVICE_REQUEST_BUTTONS) {
            apply_scan_response(service, response);
        }
    } else if (service->transport.status == WHEEL_TRANSPORT_FAILED) {
        reset_connection(service);
    }

    if (scan_active(service)) {
        start_scan(service, now_ms);
    } else {
        start_protocol(service, now_ms);
    }
}

const uint8_t *wheel_service_buttons(const WheelService *service) { return service->button_banks; }

uint8_t wheel_service_mode(const WheelService *service) { return service->protocol.mode; }

WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service) {
    return service->protocol.phase;
}
