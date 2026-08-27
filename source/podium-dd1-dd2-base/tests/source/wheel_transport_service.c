#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/wheel_link.h"
#include "wheel/transport_frame.h"
#include "wheel/transport_service.h"

static uint8_t transmitted[WHEEL_TRANSPORT_FRAME_SIZE];
static uint8_t received[WHEEL_TRANSPORT_FRAME_SIZE];
static uint8_t start_count;
static uint8_t reset_count;
static bool received_ready;

void platform_wheel_link_init(void) {}

void platform_wheel_link_reset(void) { reset_count++; }

bool platform_wheel_link_start(const uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]) {
    memcpy(transmitted, frame, sizeof(transmitted));
    start_count++;
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

static void reset_link(void) {
    memset(transmitted, 0, sizeof(transmitted));
    memset(received, 0, sizeof(received));
    start_count = 0;
    reset_count = 0;
    received_ready = false;
}

static void queue_response(const WheelTransportFrame *frame) {
    assert(wheel_transport_frame_encode(frame, received) == WHEEL_TRANSPORT_FRAME_VALID);
    received_ready = true;
}

static void test_completes_matching_transaction(void) {
    WheelTransportService service;
    WheelTransportFrame request;
    reset_link();
    wheel_transport_service_init(&service);
    const uint8_t data[3] = {1, 2, 3};

    assert(wheel_transport_service_start(&service, 3, data, sizeof(data), 100));
    assert(start_count == 1);
    assert(wheel_transport_frame_decode(transmitted, &request) == WHEEL_TRANSPORT_FRAME_VALID);
    assert(request.command == 3);
    assert(request.node == 0);
    assert(request.length == sizeof(data));
    assert(memcmp(request.data, data, sizeof(data)) == 0);

    const WheelTransportFrame response = {
        .command = 3,
        .node = 0,
        .length = 2,
        .data = {9, 8},
    };
    queue_response(&response);
    wheel_transport_service_run(&service, 101);

    const WheelTransportFrame *result = wheel_transport_service_response(&service);
    assert(service.status == WHEEL_TRANSPORT_SUCCEEDED);
    assert(service.node == 1);
    assert(result != 0);
    assert(result->length == 2);
    assert(result->data[0] == 9);
    assert(result->data[1] == 8);
}

static void test_rejects_overlapping_transaction(void) {
    WheelTransportService service;
    reset_link();
    wheel_transport_service_init(&service);

    assert(wheel_transport_service_start(&service, 3, 0, 0, 0));
    assert(!wheel_transport_service_start(&service, 5, 0, 0, 1));
    assert(start_count == 1);
}

static void test_fails_mismatched_response(void) {
    WheelTransportService service;
    reset_link();
    wheel_transport_service_init(&service);
    assert(wheel_transport_service_start(&service, 3, 0, 0, 0));

    const WheelTransportFrame response = {
        .command = 5,
    };
    queue_response(&response);
    wheel_transport_service_run(&service, 1);
    assert(service.status == WHEEL_TRANSPORT_FAILED);
    assert(wheel_transport_service_response(&service) == 0);
}

static void test_resets_timed_out_link(void) {
    WheelTransportService service;
    reset_link();
    wheel_transport_service_init(&service);
    assert(wheel_transport_service_start(&service, 3, 0, 0, 100));

    wheel_transport_service_run(&service, 109);
    assert(service.status == WHEEL_TRANSPORT_PENDING);
    wheel_transport_service_run(&service, 110);
    assert(service.status == WHEEL_TRANSPORT_FAILED);
    assert(reset_count == 1);
}

int main(void) {
    test_completes_matching_transaction();
    test_rejects_overlapping_transaction();
    test_fails_mismatched_response();
    test_resets_timed_out_link();
    return 0;
}
