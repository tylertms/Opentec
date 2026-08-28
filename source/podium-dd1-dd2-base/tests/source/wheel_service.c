#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/serial_link.h"
#include "serial/packet.h"
#include "serial/service.h"
#include "wheel/display_output.h"
#include "wheel/protocol.h"
#include "wheel/service.h"

static uint8_t transmitted[SERIAL_PACKET_SIZE];
static uint8_t received[SERIAL_PACKET_SIZE];
static SerialService transport;
static bool received_ready;

enum {
    WHEEL_BUTTON_PRIMARY_RESPONSE = 0xe0,
    WHEEL_BUTTON_SECONDARY_RESPONSE = 0xc0,
};

void platform_serial_link_init(void) {}

void platform_serial_link_reset(void) {}

bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    memcpy(transmitted, packet, sizeof(transmitted));
    return true;
}

bool platform_serial_link_take_received(uint8_t packet[SERIAL_PACKET_SIZE]) {
    if (!received_ready) {
        return false;
    }
    memcpy(packet, received, sizeof(received));
    received_ready = false;
    return true;
}

static SerialPacket request(void) {
    SerialPacket packet;
    assert(serial_packet_decode(transmitted, &packet) == SERIAL_PACKET_VALID);
    return packet;
}

static void respond_frame(const SerialPacket *packet) {
    assert(serial_packet_encode(packet->type_flags, packet->sequence, packet->payload,
                                packet->payload_length, received));
    received_ready = true;
}

static void initialize_service(WheelService *service) {
    serial_service_init(&transport);
    wheel_service_init(service, &transport);
}

static void run_service(WheelService *service, uint32_t now_ms) {
    serial_service_run(&transport, now_ms);
    wheel_service_run(service, now_ms, true);
}

static void respond_scan(uint8_t sample) {
    SerialPacket frame = {
        .type_flags = 3,
        .payload_length = SERIAL_PACKET_MAX_PAYLOAD_SIZE,
    };
    frame.payload[1] = sample;
    frame.payload[SERIAL_PACKET_MAX_PAYLOAD_SIZE - 1] = 2;
    respond_frame(&frame);
}

static void respond_protocol(uint8_t command, uint8_t mode) {
    SerialPacket frame = {
        .type_flags = 2,
        .payload_length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    frame.payload[0] = command;
    frame.payload[1] = mode;
    frame.payload[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    respond_frame(&frame);
}

static void respond_active(uint8_t flags) {
    SerialPacket frame = {
        .type_flags = 2,
        .payload_length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    frame.payload[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    frame.payload[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
    frame.payload[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(frame.payload);
    respond_frame(&frame);
}

static void respond_active_buttons(uint8_t first, uint8_t second, uint8_t third) {
    SerialPacket frame = {
        .type_flags = 2,
        .payload_length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    frame.payload[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    frame.payload[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    frame.payload[2] = first;
    frame.payload[3] = second;
    frame.payload[4] = third;
    frame.payload[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(frame.payload);
    respond_frame(&frame);
}

static uint32_t begin_scan_mode(WheelService *service, uint8_t command) {
    initialize_service(service);
    run_service(service, 0);
    assert(request().type_flags == 2);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        run_service(service, now_ms);
    }
    SerialPacket frame = request();
    assert(frame.payload[WHEEL_PROTOCOL_FLAGS_OFFSET] == WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);
    respond_protocol(command, 0);
    run_service(service, 4);
    return 5;
}

static uint32_t begin_scan(WheelService *service) {
    uint32_t now_ms = begin_scan_mode(service, WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY);
    assert(wheel_service_protocol_phase(service) == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(wheel_service_mode(service) == WHEEL_MODE_SCAN_PRIMARY);
    return now_ms;
}

typedef struct {
    uint8_t phase;
    uint8_t sample;
    uint8_t buttons[WHEEL_BUTTON_BANK_COUNT];
} ScanMapping;

static void assert_scan_mapping(const ScanMapping *mapping) {
    WheelService service;

    received_ready = false;
    uint32_t now_ms = begin_scan(&service);
    for (uint8_t observation = 0; observation < WHEEL_SCAN_SAMPLE_DEPTH; observation++) {
        SerialPacket scan = request();
        while (scan.payload[0] != mapping->phase) {
            respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
            run_service(&service, now_ms++);
            scan = request();
        }
        respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | mapping->sample);
        run_service(&service, now_ms++);
    }

    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(memcmp(buttons, mapping->buttons, sizeof(mapping->buttons)) == 0);
    assert(wheel_service_acknowledgement_input_active(&service));
}

static void test_maps_primary_scan_bits(void) {
    static const ScanMapping mappings[] = {
        {.phase = 8, .sample = 0x01, .buttons = {0x00, 0x00, 0x04}},
        {.phase = 8, .sample = 0x02, .buttons = {0x04, 0x00, 0x00}},
        {.phase = 8, .sample = 0x04, .buttons = {0x01, 0x00, 0x00}},
        {.phase = 8, .sample = 0x08, .buttons = {0x08, 0x00, 0x00}},
        {.phase = 8, .sample = 0x10, .buttons = {0x02, 0x00, 0x00}},
        {.phase = 4, .sample = 0x01, .buttons = {0x00, 0x01, 0x00}},
        {.phase = 4, .sample = 0x02, .buttons = {0x40, 0x00, 0x00}},
        {.phase = 4, .sample = 0x04, .buttons = {0x10, 0x00, 0x00}},
        {.phase = 4, .sample = 0x08, .buttons = {0x80, 0x00, 0x00}},
        {.phase = 4, .sample = 0x10, .buttons = {0x20, 0x00, 0x00}},
        {.phase = 2, .sample = 0x01, .buttons = {0x00, 0x08, 0x00}},
        {.phase = 2, .sample = 0x02, .buttons = {0x00, 0x80, 0x00}},
        {.phase = 2, .sample = 0x04, .buttons = {0x00, 0x40, 0x00}},
        {.phase = 2, .sample = 0x08, .buttons = {0x00, 0x20, 0x00}},
        {.phase = 2, .sample = 0x10, .buttons = {0x00, 0x10, 0x00}},
        {.phase = 1, .sample = 0x01, .buttons = {0x00, 0x00, 0x20}},
        {.phase = 1, .sample = 0x04, .buttons = {0x00, 0x02, 0x00}},
        {.phase = 1, .sample = 0x08, .buttons = {0x00, 0x00, 0x02}},
        {.phase = 1, .sample = 0x10, .buttons = {0x00, 0x04, 0x00}},
    };

    for (uint8_t index = 0; index < sizeof(mappings) / sizeof(mappings[0]); index++) {
        assert_scan_mapping(&mappings[index]);
    }
}

static void test_maps_secondary_scan_bit(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan_mode(&service, WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_SCANNING_SECONDARY);

    for (uint8_t observation = 0; observation < WHEEL_SCAN_SAMPLE_DEPTH; observation++) {
        SerialPacket scan = request();
        while (scan.payload[0] != WHEEL_SCAN_PHASE_FIRST) {
            respond_scan(WHEEL_BUTTON_SECONDARY_RESPONSE);
            run_service(&service, now_ms++);
            scan = request();
        }
        respond_scan(WHEEL_BUTTON_SECONDARY_RESPONSE | 0x02);
        run_service(&service, now_ms++);
    }

    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(buttons[0] == 0);
    assert(buttons[1] == 0);
    assert(buttons[2] == 0x08);
}

static void test_negotiates_before_scanning_and_maps_buttons(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);

    SerialPacket scan = request();
    assert(scan.type_flags == 3);
    assert(scan.payload_length == SERIAL_PACKET_MAX_PAYLOAD_SIZE);
    assert(scan.payload[0] == 8);
    assert(scan.payload[1] == UINT8_MAX);
    assert(scan.payload[SERIAL_PACKET_MAX_PAYLOAD_SIZE - 1] == 1);

    for (uint8_t cycle = 0; cycle < WHEEL_SCAN_SAMPLE_DEPTH; cycle++) {
        for (uint8_t phase = 0; phase < 4; phase++) {
            respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | 0x1f);
            run_service(&service, now_ms++);
        }
        const uint8_t *buttons = wheel_service_buttons(&service);
        if (cycle < WHEEL_SCAN_SAMPLE_DEPTH - 1) {
            assert(buttons[0] == 0);
            assert(buttons[1] == 0);
            assert(buttons[2] == 0);
        }
    }
    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(buttons[0] == UINT8_MAX);
    assert(buttons[1] == UINT8_MAX);
    assert(buttons[2] == 0x26);
}

static void test_releases_scan_button_on_first_zero(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);

    for (uint8_t observation = 0; observation < WHEEL_SCAN_SAMPLE_DEPTH; observation++) {
        SerialPacket scan = request();
        while (scan.payload[0] != WHEEL_SCAN_PHASE_AUXILIARY) {
            respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
            run_service(&service, now_ms++);
            scan = request();
        }
        respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | 0x01);
        run_service(&service, now_ms++);
    }
    assert((wheel_service_buttons(&service)[2] & 0x04) != 0);

    SerialPacket scan = request();
    while (scan.payload[0] != WHEEL_SCAN_PHASE_AUXILIARY) {
        respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
        run_service(&service, now_ms++);
        scan = request();
    }
    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, now_ms);
    assert((wheel_service_buttons(&service)[2] & 0x04) == 0);
}

static void test_sends_display_output_with_each_scan_phase(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);
    const WheelDisplayOutput output = {
        .glyphs = {0xa5, 0x5a, 0x40},
        .auxiliary = 0x37,
        .third_glyph_marker = true,
    };
    wheel_service_set_display_output(&service, &output);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, now_ms++);
    SerialPacket scan = request();
    assert(scan.payload[1] == (uint8_t)~0x0a);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, now_ms++);
    scan = request();
    assert(scan.payload[1] == (uint8_t)~0x36);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, now_ms++);
    scan = request();
    assert(scan.payload[1] == (uint8_t)~0xc9);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, now_ms);
    scan = request();
    assert(scan.payload[1] == (uint8_t)~0x37);
}

static void test_keeps_protocol_transport_for_packet_modes(void) {
    WheelService service;
    received_ready = false;
    initialize_service(&service);
    assert(wheel_service_clutch_paddles(&service) == 0);
    uint16_t wheel_axes[2] = {UINT16_MAX, UINT16_MAX};
    assert(!wheel_service_axis_values(&service, wheel_axes));
    assert(wheel_axes[0] == 0);
    assert(wheel_axes[1] == 0);
    run_service(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        run_service(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    run_service(&service, 4);
    SerialPacket active = {
        .type_flags = 2,
        .payload_length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    active.payload[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    active.payload[5] = 0x27;
    active.payload[6] = 0x91;
    active.payload[18] = 0x34;
    active.payload[19] = 0x12;
    active.payload[20] = 0x78;
    active.payload[21] = 0x56;
    active.payload[31] = 0x73;
    active.payload[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    active.payload[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        wheel_protocol_message_checksum(active.payload);
    respond_frame(&active);
    run_service(&service, 5);

    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_service_mode(&service) == 1);
    assert(wheel_service_axis_limit(&service) == 0x73);
    const uint8_t *clutch_paddles = wheel_service_clutch_paddles(&service);
    assert(clutch_paddles[0] == 0x27);
    assert(clutch_paddles[1] == 0x91);
    assert(wheel_service_axis_values(&service, wheel_axes));
    assert(wheel_axes[0] == 0x1234);
    assert(wheel_axes[1] == 0x5678);
    uint8_t controls[8];
    assert(wheel_service_controls(&service, controls));
    SerialPacket frame = request();
    assert(frame.type_flags == 2);
    assert(frame.payload_length == WHEEL_PROTOCOL_PACKET_SIZE);
    assert(frame.payload[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_message_valid(frame.payload));
}

static void test_publishes_packet_mode_buttons(void) {
    WheelService service;
    received_ready = false;
    initialize_service(&service);
    run_service(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        run_service(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    run_service(&service, 4);

    for (uint32_t now_ms = 5; now_ms <= 7; now_ms++) {
        respond_active_buttons(0x20, 0x04, 0x02);
        run_service(&service, now_ms);
    }

    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(buttons[0] == 0x20);
    assert(buttons[1] == 0x04);
    assert(buttons[2] == 0x02);
    assert(wheel_service_acknowledgement_input_active(&service));
}

static void test_restarts_inactive_packet_mode_at_deadline(void) {
    WheelService service;
    received_ready = false;
    initialize_service(&service);
    run_service(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        run_service(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    run_service(&service, 4);
    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    run_service(&service, 5);

    respond_active(0);
    run_service(&service, 2004);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    run_service(&service, 2005);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
    assert(request().type_flags == 2);
}

static void test_ready_packet_refreshes_activity_at_deadline(void) {
    WheelService service;
    received_ready = false;
    initialize_service(&service);
    run_service(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        run_service(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    run_service(&service, 4);
    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    run_service(&service, 5);

    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    run_service(&service, 2005);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    run_service(&service, 4004);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    run_service(&service, 4005);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
}

static void test_restarts_discovery_after_scan_timeout(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);
    service.protocol.mode_one_button_filter.samples[1][2] = 0x5a;
    service.protocol.mode_one_button_filter.next_sample = 2;
    service.protocol.mode_one_control_axis_filter.samples[2][1] = 0x6b;
    service.protocol.mode_one_control_axis_filter.next_sample = 1;
    wheel_service_configure_axis_processing(&service, 7, WHEEL_AXIS_OVERRIDE_MODE_PRIMARY, 60);
    service.protocol.axis_override_processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    service.protocol.axis_override_processor.paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_ACTIVE;
    service.protocol.axis_override_processor.x_available = true;
    service.protocol.axis_override_processor.y_available = true;
    service.protocol.axis_override_processor.overrides.axis_5.enabled = true;
    service.protocol.axis_override_processor.overrides.axis_5.value = 0x7d;
    service.protocol.capabilities.input_available = true;
    wheel_protocol_set_button_latch(&service.protocol, true, true);
    run_service(&service, now_ms + 10);
    run_service(&service, now_ms + 20);
    run_service(&service, now_ms + 30);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    run_service(&service, now_ms + 40);

    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(buttons[0] == 0);
    assert(buttons[1] == 0);
    assert(buttons[2] == 0);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
    assert(service.protocol.mode_one_button_filter.samples[1][2] == 0x5a);
    assert(service.protocol.mode_one_button_filter.next_sample == 2);
    assert(service.protocol.mode_one_control_axis_filter.samples[2][1] == 0x6b);
    assert(service.protocol.mode_one_control_axis_filter.next_sample == 1);
    assert(service.protocol.interface_mode == 7);
    assert(service.protocol.configured_axis_override_mode == WHEEL_AXIS_OVERRIDE_MODE_PRIMARY);
    assert(service.protocol.paddle_bite_point_percent == 60);
    assert(service.protocol.axis_override_processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_Y);
    assert(service.protocol.axis_override_processor.paddle_clutch_phase ==
           WHEEL_PADDLE_CLUTCH_ACTIVE);
    assert(service.protocol.axis_override_processor.x_available);
    assert(service.protocol.axis_override_processor.y_available);
    assert(!service.protocol.axis_override_processor.overrides.axis_5.enabled);
    assert(service.protocol.axis_override_processor.overrides.axis_5.value == 0);
    assert(!service.protocol.capabilities.input_available);
    assert(service.protocol.button_latch_enabled);
    assert(service.protocol.profile_transition_pending);
    assert(request().type_flags == 2);
}

static void test_defers_next_request_for_shared_serial_work(void) {
    WheelService service;
    received_ready = false;
    initialize_service(&service);

    wheel_service_run(&service, 0, false);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    wheel_service_run(&service, 1, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    assert(request().type_flags == 2);
}

static void test_initializes_rotary_input(void) {
    WheelService service;
    initialize_service(&service);

    for (uint8_t channel = 0; channel < WHEEL_ROTARY_INPUT_CHANNEL_COUNT; channel++) {
        assert(service.rotary_input.channels[channel].position == UINT8_MAX);
        assert(service.rotary_input.channels[channel].phase == WHEEL_ROTARY_PHASE_IDLE);
    }
}

static void test_routes_multi_position_mode(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.mode = 9;

    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_AUTOMATIC) ==
           TUNING_MULTI_POSITION_PULSE);

    UsbOperatingModeCommand command = {.opcode = 1, .parameters = {0x16, 2}};
    assert(wheel_service_apply_multi_position_command(&service, &command));
    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_AUTOMATIC) ==
           TUNING_MULTI_POSITION_CONSTANT);

    service.protocol.mode = 4;
    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_ENCODER);
    service.protocol.request_ready = true;
    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_service_multi_position_mode(NULL, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_ENCODER);
}

static void test_builds_direct_multi_position_input(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = 0x0f;
    service.protocol.request[6] = 3;
    service.protocol.request[7] = 10;
    service.protocol.request[14] = 0xac;
    WheelMultiPositionInput input;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.channels[0].position == 3);
    assert(input.channels[1].position == 10);
    assert(input.channels[2].position == 12);
    assert(input.channels[0].event == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[1].event == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[2].event == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[0].active);
    assert(input.channels[1].active);
    assert(input.channels[2].active);

    service.protocol.request[6] = 4;
    service.protocol.request[7] = 9;
    service.protocol.request[14] = 1;
    assert(wheel_service_multi_position_input(&service, 1, &input));
    assert(input.channels[0].event == WHEEL_ROTARY_EVENT_FORWARD);
    assert(input.channels[1].event == WHEEL_ROTARY_EVENT_BACKWARD);
    assert(input.channels[2].event == WHEEL_ROTARY_EVENT_FORWARD);
}

static void test_builds_adapter_multi_position_input(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = WHEEL_MODE_CRC_AUTHENTICATED;
    service.protocol.request[6] = 1;
    service.protocol.request[7] = 2;
    service.protocol.request[14] = 3;
    service.protocol.crc_adapter.connected = true;
    service.protocol.crc_adapter.mode = 1;
    service.protocol.crc_adapter.rotary_positions[0] = 5;
    service.protocol.crc_adapter.rotary_positions[1] = 6;
    service.protocol.crc_adapter.rotary_positions[2] = 7;
    WheelMultiPositionInput input;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.channels[0].position == 5);
    assert(input.channels[1].position == 6);
    assert(input.channels[2].position == 7);
    assert(input.channels[2].active);
}

static void test_marks_extended_multi_position_layout(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = WHEEL_MODE_REMOTE_TUNING_EXTENDED;
    WheelMultiPositionInput input;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.remap_selectors);
    assert(input.channels[2].active);
}

static void test_rejects_unavailable_multi_position_input(void) {
    WheelService service;
    initialize_service(&service);
    WheelMultiPositionInput input;

    assert(!wheel_service_multi_position_input(&service, 0, &input));
    assert(!wheel_service_multi_position_input(NULL, 0, &input));
    assert(!wheel_service_multi_position_input(&service, 0, NULL));
}

static void test_selects_extended_report_fields(void) {
    WheelService service;
    initialize_service(&service);

    assert(!wheel_service_extended_report_fields(&service));
    assert(wheel_service_accessory_flags(&service) == 0);

    service.protocol.request_ready = true;
    service.protocol.request[15] = 0xab;
    service.protocol.mode = 4;
    assert(wheel_service_extended_report_fields(&service));
    assert(wheel_service_accessory_flags(&service) == 0x0b);

    service.protocol.crc_adapter.connected = true;
    assert(!wheel_service_extended_report_fields(&service));
    service.protocol.mode = 6;
    assert(!wheel_service_extended_report_fields(&service));
    service.protocol.mode = 1;
    assert(wheel_service_extended_report_fields(&service));
    service.protocol.mode = WHEEL_MODE_CRC_AUTHENTICATED;
    assert(!wheel_service_extended_report_fields(&service));
}

static void test_reports_calibration_availability(void) {
    WheelService service;
    initialize_service(&service);

    assert(!wheel_service_calibration_available(&service));
    service.protocol.capabilities.calibration_available = true;
    assert(wheel_service_calibration_available(&service));
}

static void test_reports_mode_gated_input_capability(void) {
    WheelService service;
    initialize_service(&service);

    assert(!wheel_service_input_capability_available(&service));
    service.protocol.capabilities.input_available = true;
    service.protocol.mode = 4;
    assert(wheel_service_input_capability_available(&service));
    service.protocol.mode = 1;
    assert(!wheel_service_input_capability_available(&service));
}

static void test_exposes_axis_overrides(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.axis_override_processor.overrides.axis_6.enabled = true;
    service.protocol.axis_override_processor.overrides.axis_6.value = 0x5a;

    const WheelAxisOverrides *overrides = wheel_service_axis_overrides(&service);

    assert(overrides == &service.protocol.axis_override_processor.overrides);
    assert(overrides->axis_6.enabled);
    assert(overrides->axis_6.value == 0x5a);
}

int main(void) {
    test_maps_primary_scan_bits();
    test_maps_secondary_scan_bit();
    test_negotiates_before_scanning_and_maps_buttons();
    test_releases_scan_button_on_first_zero();
    test_sends_display_output_with_each_scan_phase();
    test_keeps_protocol_transport_for_packet_modes();
    test_publishes_packet_mode_buttons();
    test_restarts_inactive_packet_mode_at_deadline();
    test_ready_packet_refreshes_activity_at_deadline();
    test_restarts_discovery_after_scan_timeout();
    test_defers_next_request_for_shared_serial_work();
    test_initializes_rotary_input();
    test_routes_multi_position_mode();
    test_builds_direct_multi_position_input();
    test_builds_adapter_multi_position_input();
    test_marks_extended_multi_position_layout();
    test_rejects_unavailable_multi_position_input();
    test_selects_extended_report_fields();
    test_reports_calibration_availability();
    test_reports_mode_gated_input_capability();
    test_exposes_axis_overrides();
    return 0;
}
