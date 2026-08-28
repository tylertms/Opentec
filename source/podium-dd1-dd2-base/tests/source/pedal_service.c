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
    assert(service->phase == PEDAL_SERVICE_SELECT_PROTOCOL);
    pedal_service_run(service, 4);
    assert(service->phase == PEDAL_SERVICE_V3_SWITCH_WAIT);
    assert(framed_receive_count == 0);
    pedal_service_run(service, 8);
    assert(framed_receive_count == 0);
    pedal_service_run(service, 9);
    assert(framed_receive_count == 1);
    pedal_service_run(service, 10);
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
    pedal_service_run(&service, 11);

    const PedalInput *input = pedal_service_input(&service);
    assert(service.connected);
    assert(input->axes[0] == 0x1234);
    assert(input->axes[1] == 0x5678);
    assert(input->axes[2] == 0x9abc);
    assert(input->auxiliary == 0xde);
}

static void test_applies_active_brake_force(void) {
    PedalService service;
    const PedalFrame sample = {
        .type = PEDAL_FRAME_AXIS_SAMPLE,
        .payload = {1, 0, 0xe8, 3, 3, 0, 0, 4},
    };
    reset_link();
    pedal_service_init(&service);
    pedal_service_set_brake_force(&service, 50);
    connect_v3(&service);

    receive_frame(&sample);
    pedal_service_run(&service, 11);

    const PedalInput *input = pedal_service_input(&service);
    assert(input->axes[0] == 1);
    assert(input->axes[1] == 3000);
    assert(input->axes[2] == 3);
}

static void test_uses_raw_brake_during_v3_calibration(void) {
    PedalService service;
    const PedalFrame sample = {
        .type = PEDAL_FRAME_AXIS_SAMPLE,
        .payload = {1, 0, 0xe8, 3, 3, 0, 0, 4},
    };
    const PedalFrame calibration = {
        .type = 5,
        .payload = {0x04, 0x62},
    };
    reset_link();
    pedal_service_init(&service);
    pedal_service_set_brake_force(&service, 50);
    connect_v3(&service);

    receive_frame(&sample);
    pedal_service_run(&service, 11);
    assert(pedal_service_input(&service)->axes[1] == 3000);

    receive_frame(&calibration);
    pedal_service_run(&service, 12);
    assert(pedal_service_input(&service)->axes[1] == 1000);
    assert(pedal_service_v3_state(&service)->primary_calibration);
}

static void test_ignores_unknown_v3_reports_for_timeout(void) {
    PedalService service;
    const PedalFrame unknown = {
        .type = 9,
    };
    reset_link();
    pedal_service_init(&service);
    connect_v3(&service);

    receive_frame(&unknown);
    pedal_service_run(&service, 11);
    assert(!service.connected);
    pedal_service_run(&service, 15010);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
}

static void test_sends_v3_status_on_change_and_interval(void) {
    PedalService service;
    PedalFrame frame;
    const PedalProtocolStatus initial = {
        .value = 0x11,
        .first = 0x22,
        .second = 0x33,
        .scale = 0x44,
    };
    reset_link();
    pedal_service_init(&service);
    pedal_service_set_protocol_status(&service, &initial);
    connect_v3(&service);

    pedal_service_run(&service, 11);
    assert(frame_send_count == 2);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 0);
    assert(frame.payload[0] == 0x11);
    assert(frame.payload[1] == 0x22);
    assert(frame.payload[2] == 0x33);
    assert(frame.payload[3] == 0x44);

    pedal_service_run(&service, 12);
    assert(frame_send_count == 3);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 3);
    assert(frame.payload[0] == 0);
    assert(frame.payload[1] == 0);
    assert(frame.payload[2] == 0);

    PedalProtocolStatus changed = initial;
    changed.scale = 0x55;
    pedal_service_set_protocol_status(&service, &changed);
    pedal_service_run(&service, 13);
    assert(frame_send_count == 4);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 0);
    assert(frame.payload[3] == 0x55);

    pedal_service_run(&service, 513);
    assert(frame_send_count == 5);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 3);
    pedal_service_run(&service, 514);
    assert(frame_send_count == 6);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 0);
}

static void test_schedules_v3_commands_and_calibration_frames(void) {
    PedalService service;
    PedalFrame frame;
    const uint8_t input_command[PEDAL_INPUT_AXIS_COUNT] = {1, 2, 3};
    const PedalFrame calibration = {
        .type = 5,
        .payload = {0x04, 0x62},
    };
    reset_link();
    pedal_service_init(&service);
    connect_v3(&service);

    pedal_service_run(&service, 11);
    pedal_service_run(&service, 12);
    pedal_service_request_control(&service, PEDAL_V3_CONTROL_UP | PEDAL_V3_CONTROL_ENABLE |
                                                PEDAL_V3_CONTROL_AUTOMATIC);
    pedal_service_run(&service, 13);
    assert(frame_send_count == 4);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 2);
    assert(frame.payload[2] == UINT8_MAX);
    assert(frame.payload[4] == UINT8_MAX);
    assert(frame.payload[5] == 0);
    assert(service.pending_control == PEDAL_V3_CONTROL_AUTOMATIC);
    assert(pedal_service_v3_state(&service)->connection_flags == UINT8_MAX);

    pedal_service_run(&service, 14);
    assert(frame_send_count == 5);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 2);
    assert(frame.payload[2] == 0);
    assert(frame.payload[4] == 0);
    assert(frame.payload[5] == UINT8_MAX);
    assert(service.pending_control == 0);

    pedal_service_run(&service, 15);
    assert(frame_send_count == 6);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 3);
    assert(frame.payload[0] == 0);
    assert(frame.payload[1] == 0);
    assert(frame.payload[2] == 0);

    pedal_service_request_input_command(&service, input_command);
    pedal_service_run(&service, 16);
    assert(frame_send_count == 7);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 3);
    assert(frame.payload[0] == 1);
    assert(frame.payload[1] == 2);
    assert(frame.payload[2] == 3);

    receive_frame(&calibration);
    pedal_service_request_configuration(&service, 79, true);
    pedal_service_run(&service, 17);
    assert(frame_send_count == 8);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 6);
    assert(frame.payload[0] == 16);
    assert(frame.payload[1] == UINT8_MAX);
    assert(!service.configuration_pending);
    assert(service.startup_frame_count == 0);

    pedal_service_run(&service, 18);
    assert(frame_send_count == 9);
    assert(pedal_frame_decode(sent_frame, &frame) == PEDAL_FRAME_VALID);
    assert(frame.type == 0x10);
    assert(service.next_keepalive_ms == 2518);
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
    pedal_service_run(&service, 11);
    pedal_service_run(&service, 1011);

    assert(service.connected);
    assert(service.phase == PEDAL_SERVICE_V3_STREAM);
    pedal_service_run(&service, 15011);

    const PedalInput *input = pedal_service_input(&service);
    assert(!service.connected);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    assert(service.recovery_handshake);
    assert(input->axes[0] == 0);
    assert(input->axes[1] == 0);
    assert(input->axes[2] == 0);
    assert(input->auxiliary == 0);
    assert(discovery_count == 1);

    pedal_service_run(&service, 15560);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    pedal_service_run(&service, 15561);
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
        pedal_service_run(&service, 11);
    }
    pedal_service_run(&service, 1011);
    assert(service.connected);
    assert(service.phase == PEDAL_SERVICE_V3_STREAM);

    receive_frame(&sample);
    pedal_service_run(&service, 1012);
    pedal_service_run(&service, 2011);
    assert(service.connected);
    pedal_service_run(&service, 2012);
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
    pedal_service_run(&service, 4);

    assert(service.device == PEDAL_DEVICE_V4);
    assert(service.phase == PEDAL_SERVICE_V4_UNSUPPORTED);
    assert(!service.connected);
}

static void test_polls_legacy_pedal_channels(void) {
    PedalService service;
    reset_link();
    pedal_service_init(&service);

    pedal_service_run(&service, 0);
    receive_byte(PEDAL_DEVICE_V3);
    pedal_service_run(&service, 1);
    pedal_service_run(&service, 2);
    receive_byte(0x14);
    pedal_service_run(&service, 3);
    pedal_service_run(&service, 4);

    pedal_service_run(&service, 5);
    assert(sent_byte == 0x40);
    receive_byte(0xa5);
    pedal_service_run(&service, 6);
    pedal_service_run(&service, 7);
    assert(sent_byte == 0x80);
    receive_byte(0);
    pedal_service_run(&service, 8);
    pedal_service_run(&service, 9);
    assert(sent_byte == 0xc0);
    receive_byte(0xff);
    pedal_service_run(&service, 10);
    pedal_service_run(&service, 11);
    assert(sent_byte == 0);
    receive_byte(0x35);
    pedal_service_run(&service, 12);

    const PedalInput *input = pedal_service_input(&service);
    assert(input->axes[0] == 0x5a00);
    assert(input->axes[1] == 0xff00);
    assert(input->axes[2] == 0);
    assert(input->auxiliary == 0x35);
    assert(service.connected);
    assert(service.phase == PEDAL_SERVICE_LEGACY_REQUEST);
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
    test_applies_active_brake_force();
    test_uses_raw_brake_during_v3_calibration();
    test_ignores_unknown_v3_reports_for_timeout();
    test_sends_v3_status_on_change_and_interval();
    test_schedules_v3_commands_and_calibration_frames();
    test_uses_long_timeout_during_stream_startup();
    test_tightens_timeout_after_stream_startup();
    test_identifies_unsupported_v4_transport();
    test_polls_legacy_pedal_channels();
    test_retries_after_discovery_timeout();
    test_selects_analog_input_after_discovery_timeout();
    return 0;
}
