#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pedal/frame.h"
#include "pedal/service.h"
#include "platform/pedal_link.h"

static uint8_t sent_byte;
static uint8_t sent_frame[PEDAL_FRAME_SIZE];
static uint8_t received_byte;
static uint8_t received_frame[PEDAL_FRAME_SIZE];
static uint8_t byte_send_count;
static uint8_t frame_send_count;
static uint8_t discovery_count;
static uint8_t analog_count;
static uint8_t framed_receive_count;
static bool byte_ready;
static bool frame_ready;

void platform_pedal_link_init(void) {}

void platform_pedal_link_begin_analog(void) { analog_count++; }

void platform_pedal_link_begin_discovery(void) { discovery_count++; }

void platform_pedal_link_begin_framed_receive(void) { framed_receive_count++; }

bool platform_pedal_link_send_byte(uint8_t value) {
    sent_byte = value;
    byte_send_count++;
    return true;
}

bool platform_pedal_link_send_frame(const uint8_t frame[PEDAL_FRAME_SIZE]) {
    memcpy(sent_frame, frame, sizeof(sent_frame));
    frame_send_count++;
    return true;
}

bool platform_pedal_link_take_byte(uint8_t *value) {
    if (!byte_ready) {
        return false;
    }
    *value = received_byte;
    byte_ready = false;
    return true;
}

bool platform_pedal_link_take_frame(uint8_t frame[PEDAL_FRAME_SIZE]) {
    if (!frame_ready) {
        return false;
    }
    memcpy(frame, received_frame, sizeof(received_frame));
    frame_ready = false;
    return true;
}

static void reset_link(void) {
    sent_byte = 0;
    memset(sent_frame, 0, sizeof(sent_frame));
    received_byte = 0;
    memset(received_frame, 0, sizeof(received_frame));
    byte_send_count = 0;
    frame_send_count = 0;
    discovery_count = 0;
    analog_count = 0;
    framed_receive_count = 0;
    byte_ready = false;
    frame_ready = false;
}

static void receive_byte(uint8_t value) {
    received_byte = value;
    byte_ready = true;
}

static void receive_frame(const PedalFrame *frame) {
    pedal_frame_encode(frame, received_frame);
    frame_ready = true;
}

static void connect_v3(PedalService *service) {
    pedal_service_run(service, 0);
    assert(sent_byte == 0x0a);
    receive_byte(PEDAL_DEVICE_V3);
    pedal_service_run(service, 1);
    pedal_service_run(service, 2);
    assert(sent_byte == 0x05);
    receive_byte(0x15);
    pedal_service_run(service, 3);
    assert(framed_receive_count == 0);
    pedal_service_run(service, 7);
    assert(framed_receive_count == 0);
    pedal_service_run(service, 8);
    assert(framed_receive_count == 1);
    pedal_service_run(service, 9);
    assert(service->phase == PEDAL_SERVICE_V3_STREAM);
}

static void test_connects_and_publishes_v3_input(void) {
    PedalService service;
    PedalFrame handshake;
    reset_link();
    pedal_service_init(&service);
    connect_v3(&service);

    assert(frame_send_count == 1);
    assert(pedal_frame_decode(sent_frame, &handshake) == PEDAL_FRAME_VALID);
    assert(handshake.type == 2);
    assert(handshake.payload[0] == 0xff);
    assert(handshake.payload[1] == 0);

    const PedalFrame sample = {
        .type = PEDAL_FRAME_AXIS_SAMPLE,
        .payload = {0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0, 0xde},
    };
    receive_frame(&sample);
    pedal_service_run(&service, 10);

    const PedalInput *input = pedal_service_input(&service);
    assert(service.connected);
    assert(input->axes[0] == 0x1234);
    assert(input->axes[1] == 0x5678);
    assert(input->axes[2] == 0x9abc);
    assert(input->auxiliary == 0xde);
}

static void test_uses_long_timeout_during_stream_startup(void) {
    PedalService service;
    reset_link();
    pedal_service_init(&service);
    connect_v3(&service);

    const PedalFrame sample = {
        .type = PEDAL_FRAME_AXIS_SAMPLE,
        .payload = {1, 0, 2, 0, 3, 0, 0, 4},
    };
    receive_frame(&sample);
    pedal_service_run(&service, 10);
    pedal_service_run(&service, 1010);

    assert(service.connected);
    assert(service.phase == PEDAL_SERVICE_V3_STREAM);
    pedal_service_run(&service, 15010);

    const PedalInput *input = pedal_service_input(&service);
    assert(!service.connected);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    assert(input->axes[0] == 0);
    assert(input->axes[1] == 0);
    assert(input->axes[2] == 0);
    assert(input->auxiliary == 0);
    assert(discovery_count == 1);

    pedal_service_run(&service, 15559);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    pedal_service_run(&service, 15560);
    assert(service.phase == PEDAL_SERVICE_DETECT_REQUEST);
}

static void test_tightens_timeout_after_stream_startup(void) {
    PedalService service;
    const PedalFrame sample = {
        .type = PEDAL_FRAME_AXIS_SAMPLE,
        .payload = {1, 0, 2, 0, 3, 0, 0, 4},
    };
    reset_link();
    pedal_service_init(&service);
    connect_v3(&service);

    for (uint16_t frame = 0; frame < 250; frame++) {
        receive_frame(&sample);
        pedal_service_run(&service, 10);
    }
    pedal_service_run(&service, 1010);
    assert(service.connected);
    assert(service.phase == PEDAL_SERVICE_V3_STREAM);

    receive_frame(&sample);
    pedal_service_run(&service, 1011);
    pedal_service_run(&service, 2010);
    assert(service.connected);
    pedal_service_run(&service, 2011);
    assert(!service.connected);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
}

static void test_identifies_unsupported_v4_transport(void) {
    PedalService service;
    reset_link();
    pedal_service_init(&service);

    pedal_service_run(&service, 0);
    receive_byte(PEDAL_DEVICE_V4);
    pedal_service_run(&service, 1);
    pedal_service_run(&service, 2);
    assert(sent_byte == 0x06);
    receive_byte(0x26);
    pedal_service_run(&service, 3);

    assert(service.device == PEDAL_DEVICE_V4);
    assert(service.phase == PEDAL_SERVICE_V4_UNSUPPORTED);
    assert(!service.connected);
}

static void test_retries_after_discovery_timeout(void) {
    PedalService service;
    reset_link();
    pedal_service_init(&service);

    pedal_service_run(&service, 0);
    pedal_service_run(&service, 99);
    assert(service.phase == PEDAL_SERVICE_DETECT_RESPONSE);
    pedal_service_run(&service, 100);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    assert(discovery_count == 1);
}

static void test_selects_analog_input_after_discovery_timeout(void) {
    PedalService service;
    const uint16_t samples[PEDAL_INPUT_AXIS_COUNT] = {0, 0x0800, 0x0fff};
    reset_link();
    pedal_service_init(&service);
    pedal_service_set_analog_samples(&service, samples);

    pedal_service_run(&service, 0);
    pedal_service_run(&service, 100);

    const PedalInput *input = pedal_service_input(&service);
    assert(service.phase == PEDAL_SERVICE_ANALOG);
    assert(service.connected);
    assert(analog_count == 1);
    assert(discovery_count == 0);
    assert(input->axes[0] == UINT16_MAX);
    assert(input->axes[2] == 0);
}

int main(void) {
    test_connects_and_publishes_v3_input();
    test_uses_long_timeout_during_stream_startup();
    test_tightens_timeout_after_stream_startup();
    test_identifies_unsupported_v4_transport();
    test_retries_after_discovery_timeout();
    test_selects_analog_input_after_discovery_timeout();
    return 0;
}
