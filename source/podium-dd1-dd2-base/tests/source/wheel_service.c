#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/wheel_link.h"
#include "wheel/protocol.h"
#include "wheel/service.h"
#include "wheel/transport_frame.h"

static uint8_t transmitted[WHEEL_TRANSPORT_FRAME_SIZE];
static uint8_t received[WHEEL_TRANSPORT_FRAME_SIZE];
static bool received_ready;

void platform_wheel_link_init(void) {}

void platform_wheel_link_reset(void) {}

bool platform_wheel_link_start(const uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]) {
    memcpy(transmitted, frame, sizeof(transmitted));
    return true;
}

bool platform_wheel_link_take_received(uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]) {
    if (!received_ready) {
        return false;
    }
    memcpy(frame, received, sizeof(received));
    received_ready = false;
    return true;
}

static WheelTransportFrame request(void) {
    WheelTransportFrame frame;
    assert(wheel_transport_frame_decode(transmitted, &frame) == WHEEL_TRANSPORT_FRAME_VALID);
    return frame;
}

static void respond_frame(WheelTransportFrame *frame) {
    assert(wheel_transport_frame_encode(frame, received) == WHEEL_TRANSPORT_FRAME_VALID);
    received_ready = true;
}

static void respond_scan(uint8_t sample) {
    WheelTransportFrame frame = {
        .command = 3,
        .length = WHEEL_TRANSPORT_PAYLOAD_SIZE,
    };
    frame.data[1] = sample;
    frame.data[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] = 2;
    respond_frame(&frame);
}

static void respond_status(void) {
    WheelTransportFrame frame = {
        .command = 5,
        .length = WHEEL_STATUS_RESPONSE_SIZE,
        .data = {1, 2, 0x34, 0x12, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0xaa},
    };
    respond_frame(&frame);
}

static void respond_protocol(uint8_t command, uint8_t mode) {
    WheelTransportFrame frame = {
        .command = 2,
        .length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    frame.data[0] = command;
    frame.data[1] = mode;
    frame.data[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    respond_frame(&frame);
}

static void begin_scan(WheelService *service) {
    wheel_service_init(service);
    wheel_service_run(service, 0);
    assert(request().command == 5);
    respond_status();
    wheel_service_run(service, 1);
    assert(request().command == 2);
    respond_protocol(0, 0);
    wheel_service_run(service, 2);
    WheelTransportFrame frame = request();
    assert(frame.data[WHEEL_PROTOCOL_FLAGS_OFFSET] == WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY, 0);
    wheel_service_run(service, 3);
    assert(wheel_service_protocol_phase(service) == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(wheel_service_mode(service) == WHEEL_MODE_SCAN_PRIMARY);
}

static void test_negotiates_before_scanning_and_maps_buttons(void) {
    WheelService service;
    received_ready = false;
    begin_scan(&service);

    const WheelStatus *status = wheel_service_status(&service);
    assert(status != 0);
    assert(status->accessory_value == 0x1234);
    assert(status->runtime_seconds == 1);
    assert(status->runtime_counter == 2);
    assert(status->trailing_status == 3);

    WheelTransportFrame scan = request();
    assert(scan.command == 3);
    assert(scan.length == WHEEL_TRANSPORT_PAYLOAD_SIZE);
    assert(scan.data[0] == 8);
    assert(scan.data[1] == UINT8_MAX);
    assert(scan.data[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] == 1);

    respond_scan(0xe0 | 0x1f);
    wheel_service_run(&service, 4);
    scan = request();
    assert(scan.data[0] == 4);
    const uint8_t *buttons = wheel_service_buttons(&service);
    assert((buttons[0] & 0x0f) == 0x0f);
    assert((buttons[2] & 0x04) != 0);

    respond_scan(0xe0 | 0x1f);
    wheel_service_run(&service, 5);
    scan = request();
    assert(scan.data[0] == 2);
    buttons = wheel_service_buttons(&service);
    assert((buttons[0] & 0xf0) == 0xf0);
    assert((buttons[1] & 0x01) != 0);

    respond_scan(0xe0 | 0x1f);
    wheel_service_run(&service, 6);
    scan = request();
    assert(scan.data[0] == 1);
    buttons = wheel_service_buttons(&service);
    assert((buttons[1] & 0xf8) == 0xf8);

    respond_scan(0xe0 | 0x1f);
    wheel_service_run(&service, 7);
    scan = request();
    assert(scan.data[0] == 8);
    buttons = wheel_service_buttons(&service);
    assert((buttons[1] & 0x06) == 0x06);
    assert((buttons[2] & 0x22) == 0x22);
}

static void test_keeps_protocol_transport_for_packet_modes(void) {
    WheelService service;
    received_ready = false;
    wheel_service_init(&service);
    wheel_service_run(&service, 0);
    respond_status();
    wheel_service_run(&service, 1);
    respond_protocol(0, 0);
    wheel_service_run(&service, 2);
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    wheel_service_run(&service, 3);

    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_service_mode(&service) == 1);
    WheelTransportFrame frame = request();
    assert(frame.command == 2);
    assert(frame.length == WHEEL_PROTOCOL_PACKET_SIZE);
    assert(frame.data[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_message_valid(frame.data));
}

static void test_restarts_discovery_after_scan_timeout(void) {
    WheelService service;
    received_ready = false;
    begin_scan(&service);
    wheel_service_run(&service, 13);

    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(buttons[0] == 0);
    assert(buttons[1] == 0);
    assert(buttons[2] == 0);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
    assert(request().command == 5);
}

int main(void) {
    test_negotiates_before_scanning_and_maps_buttons();
    test_keeps_protocol_transport_for_packet_modes();
    test_restarts_discovery_after_scan_timeout();
    return 0;
}
