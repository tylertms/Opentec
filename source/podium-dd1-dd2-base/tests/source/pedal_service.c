#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pedal/frame.h"
#include "pedal/service.h"
#include "platform/pedal_link.h"
#include "transfer/frame.h"

static uint8_t sent_byte;
static uint8_t sent_frame[PEDAL_FRAME_SIZE];
static uint8_t received_byte;
static uint8_t received_frame[PEDAL_FRAME_SIZE];
static uint8_t sent_transfer[TRANSFER_FRAME_MAX_ENCODED_SIZE];
static uint8_t received_transfer[TRANSFER_FRAME_MAX_RECEIVED_SIZE];
static uint16_t sent_transfer_length;
static uint16_t received_transfer_length;
static uint8_t byte_send_count;
static uint8_t frame_send_count;
static uint8_t discovery_count;
static uint8_t analog_count;
static uint8_t framed_receive_count;
static uint8_t transfer_receive_count;
static uint8_t transfer_send_count;
static bool byte_ready;
static bool frame_ready;
static bool transfer_busy;

void platform_pedal_link_init(void) {}

void platform_pedal_link_begin_analog(void) { analog_count++; }

void platform_pedal_link_begin_discovery(void) { discovery_count++; }

void platform_pedal_link_begin_framed_receive(void) { framed_receive_count++; }

void platform_pedal_link_begin_transfer_receive(void) { transfer_receive_count++; }

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

bool platform_pedal_link_send_transfer(const uint8_t *data, uint16_t length) {
    if (transfer_busy || length > sizeof(sent_transfer)) {
        return false;
    }
    memcpy(sent_transfer, data, length);
    sent_transfer_length = length;
    transfer_send_count++;
    return true;
}

bool platform_pedal_link_transmit_busy(void) { return transfer_busy; }

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

uint16_t platform_pedal_link_take_transfer(uint8_t *data, uint16_t capacity) {
    if (received_transfer_length == 0 || received_transfer_length > capacity) {
        return 0;
    }
    uint16_t length = received_transfer_length;
    memcpy(data, received_transfer, length);
    received_transfer_length = 0;
    return length;
}

static void reset_link(void) {
    sent_byte = 0;
    memset(sent_frame, 0, sizeof(sent_frame));
    received_byte = 0;
    memset(received_frame, 0, sizeof(received_frame));
    memset(sent_transfer, 0, sizeof(sent_transfer));
    memset(received_transfer, 0, sizeof(received_transfer));
    sent_transfer_length = 0;
    received_transfer_length = 0;
    byte_send_count = 0;
    frame_send_count = 0;
    discovery_count = 0;
    analog_count = 0;
    framed_receive_count = 0;
    transfer_receive_count = 0;
    transfer_send_count = 0;
    byte_ready = false;
    frame_ready = false;
    transfer_busy = false;
}

static void receive_byte(uint8_t value) {
    received_byte = value;
    byte_ready = true;
}

static void receive_frame(const PedalFrame *frame) {
    pedal_frame_encode(frame, received_frame);
    frame_ready = true;
}

static void receive_transfer(uint16_t command, const uint8_t *payload, uint8_t payload_length) {
    received_transfer_length =
        transfer_frame_encode_values(command, payload, payload_length, received_transfer);
}

static void complete_v4_request(PedalService *service, uint32_t ack_time, uint32_t response_time) {
    receive_transfer(transfer_status_command(0, 0), NULL, 0);
    pedal_service_run(service, ack_time);
    receive_transfer(transfer_data_command(0, 0, 0), NULL, 0);
    pedal_service_run(service, response_time);
}

static void assert_v4_tuning_request(uint8_t offset, uint8_t value) {
    TransferFrame request;
    assert(transfer_frame_decode(sent_transfer, sent_transfer_length, &request) ==
           TRANSFER_FRAME_VALID);
    assert(request.command == transfer_data_command(0, 0, 0));
    assert(request.payload_length == PEDAL_V4_TUNING_REQUEST_SIZE);
    assert(request.payload[19] == offset);
    assert(request.payload[20] == value);
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

static void connect_v4(PedalService *service) {
    pedal_service_run(service, 0);
    receive_byte(PEDAL_DEVICE_V4);
    pedal_service_run(service, 1);
    pedal_service_run(service, 2);
    assert(sent_byte == 0x06);
    receive_byte(0x26);
    pedal_service_run(service, 3);
    pedal_service_run(service, 4);
    assert(service->phase == PEDAL_SERVICE_V4_START);
    pedal_service_run(service, 5);
    assert(service->phase == PEDAL_SERVICE_V4_STREAM);
    assert(transfer_receive_count == 1);
}

static void test_polls_and_publishes_v4_input(void) {
    PedalService service;
    TransferFrame request;
    static const uint8_t expected_request[] = {
        0x12, 0x0a, 0x00, 0x00, 0x02, 0x08, 0x00, 0x00, 0x02, 0x18, 0x00,
        0x00, 0x01, 0x20, 0x00, 0x00, 0x08, 0xaa, 0x00, 0x00, 0x01,
    };
    uint8_t status[45] = {0};
    status[25] = 0x0a;
    status[26] = 5;
    status[27] = 0x08;
    status[28] = 2;
    status[29] = 0x10;
    status[30] = 0xb4;
    status[31] = 0x24;
    status[32] = 0x0a;
    status[33] = 5;
    status[34] = 0x08;
    status[35] = 1;
    status[36] = 0x10;
    status[37] = 0xe8;
    status[38] = 0x07;
    status[39] = 0x0a;
    status[40] = 4;
    status[41] = 0x08;
    status[42] = 3;
    status[43] = 0x10;
    status[44] = 0x56;
    reset_link();
    pedal_service_init(&service);
    connect_v4(&service);

    pedal_service_run(&service, 6);
    assert(transfer_send_count == 1);
    assert(transfer_frame_decode(sent_transfer, sent_transfer_length, &request) ==
           TRANSFER_FRAME_VALID);
    assert(request.command == transfer_data_command(0, 0, 0));
    assert(request.payload_length == sizeof(expected_request));
    assert(memcmp(request.payload, expected_request, sizeof(expected_request)) == 0);

    receive_transfer(transfer_status_command(0, 0), NULL, 0);
    pedal_service_run(&service, 7);
    assert(service.v4.outbound_pending == false);

    receive_transfer(transfer_data_command(0, 0, 0), status, sizeof(status));
    pedal_service_run(&service, 8);

    assert(service.device == PEDAL_DEVICE_V4);
    assert(service.phase == PEDAL_SERVICE_V4_STREAM);
    assert(service.connected);
    assert(service.input.axes[0] == 0x1234);
    assert(service.input.axes[1] == 0x03e8);
    assert(service.input.axes[2] == 0x56);
    assert(transfer_send_count == 2);

    pedal_service_run(&service, 20);
    assert(transfer_send_count == 2);
    pedal_service_run(&service, 21);
    assert(transfer_send_count == 2);
    pedal_service_run(&service, 22);
    assert(transfer_send_count == 3);
}

static void test_sends_v4_tuning_in_protocol_order(void) {
    PedalService service;
    const PedalV4Tuning tuning = {
        .brake_force = 50,
        .clutch_curve = 3,
        .brake_curve = 2,
        .throttle_curve = 5,
    };
    reset_link();
    pedal_service_init(&service);
    pedal_service_set_v4_tuning(&service, tuning);
    connect_v4(&service);

    pedal_service_run(&service, 6);
    complete_v4_request(&service, 7, 8);
    pedal_service_run(&service, 9);

    pedal_service_run(&service, 10);
    assert_v4_tuning_request(32, 50);
    complete_v4_request(&service, 11, 12);

    pedal_service_run(&service, 13);
    assert_v4_tuning_request(24, 3);
    complete_v4_request(&service, 14, 15);

    pedal_service_run(&service, 16);
    assert_v4_tuning_request(16, 2);
    complete_v4_request(&service, 17, 18);

    pedal_service_run(&service, 19);
    assert_v4_tuning_request(8, 5);
    complete_v4_request(&service, 20, 21);

    assert(service.v4_tuning_pending == 0);
    assert(service.v4_phase == PEDAL_V4_PHASE_STATUS);
}

static void test_prioritizes_throttle_before_brake_at_v4_selection(void) {
    PedalService service;
    const PedalV4Tuning tuning = {
        .brake_curve = 2,
        .throttle_curve = 5,
    };
    reset_link();
    pedal_service_init(&service);
    pedal_service_set_v4_tuning(&service, tuning);
    connect_v4(&service);

    pedal_service_run(&service, 6);
    complete_v4_request(&service, 7, 8);
    pedal_service_run(&service, 9);
    pedal_service_run(&service, 10);

    assert_v4_tuning_request(8, 5);
}

static void test_reconnects_after_v4_transfer_timeout(void) {
    PedalService service;
    reset_link();
    pedal_service_init(&service);
    connect_v4(&service);

    pedal_service_run(&service, 205);
    assert(service.phase == PEDAL_SERVICE_V4_STREAM);
    pedal_service_run(&service, 206);

    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    assert(!service.connected);
    assert(discovery_count == 1);
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
    assert(input->axes[0] == 0);
    assert(input->axes[2] == 0);

    pedal_service_set_analog_samples(&service, samples);
    assert(input->axes[0] == 45);

    const uint16_t disconnected[PEDAL_INPUT_AXIS_COUNT] = {0, 0, 0};
    pedal_service_set_analog_samples(&service, disconnected);
    assert(service.phase == PEDAL_SERVICE_RECONNECT_WAIT);
    assert(!service.connected);
    assert(discovery_count == 1);
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
    test_polls_and_publishes_v4_input();
    test_sends_v4_tuning_in_protocol_order();
    test_prioritizes_throttle_before_brake_at_v4_selection();
    test_reconnects_after_v4_transfer_timeout();
    test_polls_legacy_pedal_channels();
    test_retries_after_discovery_timeout();
    test_selects_analog_input_after_discovery_timeout();
    return 0;
}
