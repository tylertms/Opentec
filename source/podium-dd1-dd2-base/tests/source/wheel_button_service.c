#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/wheel_link.h"
#include "wheel/button_service.h"
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

static void respond(uint8_t sample) {
    WheelTransportFrame frame = {
        .command = 3,
        .length = WHEEL_TRANSPORT_PAYLOAD_SIZE,
    };
    frame.data[1] = sample;
    frame.data[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] = 2;
    assert(wheel_transport_frame_encode(&frame, received) == WHEEL_TRANSPORT_FRAME_VALID);
    received_ready = true;
}

static void respond_status(void) {
    WheelTransportFrame frame = {
        .command = 5,
        .length = WHEEL_STATUS_RESPONSE_SIZE,
        .data = {1, 2, 0x34, 0x12, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0xaa},
    };
    assert(wheel_transport_frame_encode(&frame, received) == WHEEL_TRANSPORT_FRAME_VALID);
    received_ready = true;
}

static void test_cycles_scan_phases_and_maps_buttons(void) {
    WheelButtonService service;
    received_ready = false;
    wheel_button_service_init(&service);

    wheel_button_service_run(&service, 0);
    WheelTransportFrame scan = request();
    assert(scan.command == 5);
    assert(scan.length == 1);
    assert(scan.data[0] == 0);
    respond_status();
    wheel_button_service_run(&service, 1);
    const WheelStatus *status = wheel_button_service_status(&service);
    assert(status != 0);
    assert(status->accessory_value == 0x1234);
    assert(status->runtime_seconds == 1);
    assert(status->runtime_counter == 2);
    assert(status->trailing_status == 3);

    scan = request();
    assert(scan.command == 3);
    assert(scan.length == WHEEL_TRANSPORT_PAYLOAD_SIZE);
    assert(scan.data[0] == 8);
    assert(scan.data[1] == UINT8_MAX);
    assert(scan.data[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] == 1);

    respond(0xe0 | 0x1f);
    wheel_button_service_run(&service, 2);
    scan = request();
    assert(scan.data[0] == 4);
    const uint8_t *buttons = wheel_button_service_buttons(&service);
    assert((buttons[0] & 0x0f) == 0x0f);
    assert((buttons[2] & 0x04) != 0);

    respond(0xe0 | 0x1f);
    wheel_button_service_run(&service, 3);
    scan = request();
    assert(scan.data[0] == 2);
    buttons = wheel_button_service_buttons(&service);
    assert((buttons[0] & 0xf0) == 0xf0);
    assert((buttons[1] & 0x01) != 0);

    respond(0xe0 | 0x1f);
    wheel_button_service_run(&service, 4);
    scan = request();
    assert(scan.data[0] == 1);
    buttons = wheel_button_service_buttons(&service);
    assert((buttons[1] & 0xf8) == 0xf8);

    respond(0xc0 | 0x1f);
    wheel_button_service_run(&service, 5);
    scan = request();
    assert(scan.data[0] == 8);
    buttons = wheel_button_service_buttons(&service);
    assert((buttons[1] & 0x06) == 0x06);
    assert((buttons[2] & 0x2a) == 0x2a);
}

static void test_clears_buttons_after_link_timeout(void) {
    WheelButtonService service;
    received_ready = false;
    wheel_button_service_init(&service);
    wheel_button_service_run(&service, 0);
    respond_status();
    wheel_button_service_run(&service, 1);
    respond(0xe0 | 0x1f);
    wheel_button_service_run(&service, 2);
    wheel_button_service_run(&service, 12);

    const uint8_t *buttons = wheel_button_service_buttons(&service);
    assert(buttons[0] == 0);
    assert(buttons[1] == 0);
    assert(buttons[2] == 0);
}

int main(void) {
    test_cycles_scan_phases_and_maps_buttons();
    test_clears_buttons_after_link_timeout();
    return 0;
}
