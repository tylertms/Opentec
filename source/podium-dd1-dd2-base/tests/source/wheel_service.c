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
        .third_glyph_marker = true,
    };
    wheel_service_set_display_output(&service, &output);
    const WheelVibrationOutput vibration = {.channels = {0x01, 0x01}};
    wheel_service_set_vibration_output(&service, &vibration);
    assert(service.protocol.adapter_output.display.glyphs[0] == 0xa5);
    assert(service.protocol.adapter_output.display.glyphs[1] == 0x5a);
    assert(service.protocol.adapter_output.display.glyphs[2] == 0x40);

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
    assert(scan.payload[1] == (uint8_t)~0x11);
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
    service.protocol.display_filter.samples[1][4] = 0x6c;
    service.protocol.display_filter.next_sample = 2;
    service.protocol.remapped_filter.samples[2][1] = 0x6d;
    service.protocol.remapped_filter.next_sample = 1;
    service.protocol.alternate_filter.samples[1][2] = 0x6e;
    service.protocol.alternate_filter.next_sample = 2;
    service.protocol.alternate_output.payload[4] = 0x6f;
    service.protocol.alternate_output.payload_pending = true;
    service.protocol.packed_filter.samples[2][0] = 0x7c;
    service.protocol.packed_filter.next_sample = 1;
    wheel_service_configure_axis_processing(&service, 7, WHEEL_AXIS_OVERRIDE_MODE_PRIMARY, 60, 123);
    service.protocol.axis_override_processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    service.protocol.axis_override_processor.paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_ACTIVE;
    service.protocol.axis_override_processor.paddle_adjustment_deadline_ms = 456;
    service.protocol.axis_override_processor.paddle_bite_point_report_pending = true;
    service.protocol.axis_override_processor.paddle_bite_point_commit_pending = true;
    service.protocol.axis_override_processor.x_available = true;
    service.protocol.axis_override_processor.y_available = true;
    service.protocol.axis_override_processor.overrides.axis_5.enabled = true;
    service.protocol.axis_override_processor.overrides.axis_5.value = 0x7d;
    service.protocol.capabilities.input_available = true;
    wheel_protocol_set_button_latch(&service.protocol, true, true);
    wheel_service_set_host_capability(&service, true);
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
    assert(service.protocol.display_filter.samples[1][4] == 0x6c);
    assert(service.protocol.display_filter.next_sample == 2);
    assert(service.protocol.remapped_filter.samples[2][1] == 0x6d);
    assert(service.protocol.remapped_filter.next_sample == 1);
    assert(service.protocol.alternate_filter.samples[1][2] == 0x6e);
    assert(service.protocol.alternate_filter.next_sample == 2);
    assert(service.protocol.alternate_output.payload[4] == 0x6f);
    assert(service.protocol.alternate_output.payload_pending);
    assert(service.protocol.packed_filter.samples[2][0] == 0x7c);
    assert(service.protocol.packed_filter.next_sample == 1);
    assert(service.protocol.interface_mode == 7);
    assert(service.protocol.configured_axis_override_mode == WHEEL_AXIS_OVERRIDE_MODE_PRIMARY);
    assert(service.protocol.paddle_bite_point_percent == 60);
    assert(service.protocol.now_ms == 123);
    assert(service.protocol.axis_override_processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_Y);
    assert(service.protocol.axis_override_processor.paddle_clutch_phase ==
           WHEEL_PADDLE_CLUTCH_ACTIVE);
    assert(service.protocol.axis_override_processor.paddle_adjustment_deadline_ms == 456);
    uint8_t adjusted_bite_point_percent = 0;
    assert(wheel_service_take_bite_point_report(&service, &adjusted_bite_point_percent));
    assert(adjusted_bite_point_percent == 60);
    assert(wheel_service_take_bite_point(&service, &adjusted_bite_point_percent));
    assert(adjusted_bite_point_percent == 60);
    assert(service.protocol.axis_override_processor.x_available);
    assert(service.protocol.axis_override_processor.y_available);
    assert(!service.protocol.axis_override_processor.overrides.axis_5.enabled);
    assert(service.protocol.axis_override_processor.overrides.axis_5.value == 0);
    assert(!service.protocol.capabilities.input_available);
    assert(service.protocol.button_latch_enabled);
    assert(service.protocol.profile_transition_pending);
    assert(service.protocol.host_capability_enabled);
    assert((wheel_protocol_response(&service.protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] &
            WHEEL_PROTOCOL_HOST_CAPABILITY) != 0);
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

static void test_forces_and_reports_protocol_exchange(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;

    assert(!wheel_service_start_protocol_exchange(NULL, 0));
    assert(wheel_service_start_protocol_exchange(&service, 10));
    assert(!wheel_service_start_protocol_exchange(&service, 10));
    assert(request().type_flags == 2);
    assert(!wheel_service_take_protocol_exchange_completed(&service));

    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, WHEEL_MODE_SCAN_PRIMARY);
    serial_service_run(&transport, 11);
    wheel_service_run(&service, 11, false);
    assert(wheel_service_take_protocol_exchange_completed(&service));
    assert(!wheel_service_take_protocol_exchange_completed(&service));
    assert(!wheel_service_take_protocol_exchange_completed(NULL));
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
    assert(!wheel_service_multi_position_supported(&service));
    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_ENCODER);
    service.protocol.request_ready = true;
    assert(wheel_service_multi_position_supported(&service));
    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_service_multi_position_mode(NULL, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(!wheel_service_multi_position_supported(NULL));
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
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    service.protocol.adapter.rotary_positions[0] = 5;
    service.protocol.adapter.rotary_positions[1] = 6;
    service.protocol.adapter.rotary_positions[2] = 7;
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

static void test_filters_adapter_remote_tuning_active_state(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.mode = 4;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 0;

    wheel_service_queue_adapter_remote_tuning_active(&service, true);
    assert(service.adapter_commands.remote_tuning_active == 1);
    assert(service.adapter_commands.remote_tuning_active_pending);

    service.protocol.adapter.mode = 1;
    wheel_service_queue_adapter_remote_tuning_active(&service, true);
    assert(service.adapter_commands.remote_tuning_active == 0);

    service.protocol.mode = WHEEL_MODE_REMOTE_TUNING_LEGACY;
    service.protocol.adapter.mode = 0;
    wheel_service_queue_adapter_remote_tuning_active(&service, true);
    assert(service.adapter_commands.remote_tuning_active == 0);

    service.protocol.mode = 4;
    service.protocol.adapter.connected = false;
    wheel_service_queue_adapter_remote_tuning_active(&service, true);
    assert(service.adapter_commands.remote_tuning_active == 0);
}

static void test_retains_adapter_display_state_across_command_resets(void) {
    WheelService service;
    initialize_service(&service);

    wheel_service_queue_adapter_display_state(&service, 0x39);
    assert(service.adapter_display_state == 0x39);
    assert(service.adapter_commands.display_state == 0x39);
    assert(service.adapter_commands.display_state_pending);

    wheel_service_reset_adapter_commands(&service);
    assert(service.adapter_commands.display_state == 0x39);
    assert(service.adapter_commands.display_state_pending);
}

static void test_mirrors_extended_adapter_output_reports(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.adapter.mode = 1;
    uint8_t arguments[1 + WHEEL_OUTPUT_REPORT_FOUR_SIZE];
    arguments[0] = WHEEL_OUTPUT_REPORT_ACTION_FOUR;
    for (uint8_t index = 1; index < sizeof(arguments); index++) {
        arguments[index] = index;
    }

    wheel_service_apply_output_report(&service, arguments);

    assert(service.adapter_commands.report_four_pending);
    assert(memcmp(service.adapter_commands.report_four, arguments + 1,
                  WHEEL_OUTPUT_REPORT_FOUR_SIZE) == 0);
    assert(memcmp(service.protocol.output_reports.report_four, arguments + 1,
                  WHEEL_OUTPUT_REPORT_FOUR_SIZE) == 0);
}

static void test_mirrors_standard_adapter_output_reports(void) {
    WheelService service;
    initialize_service(&service);
    uint8_t arguments[1 + WHEEL_OUTPUT_REPORT_TWO_SIZE];
    arguments[0] = WHEEL_OUTPUT_REPORT_ACTION_TWO;
    for (uint8_t index = 1; index < sizeof(arguments); index++) {
        arguments[index] = (uint8_t)(0x20u + index);
    }

    wheel_service_apply_output_report(&service, arguments);
    assert(service.adapter_commands.report_two_pending);
    assert(memcmp(service.adapter_commands.report_two, arguments + 1,
                  WHEEL_OUTPUT_REPORT_TWO_SIZE) == 0);

    arguments[0] = WHEEL_OUTPUT_REPORT_ACTION_ONE;
    wheel_service_apply_output_report(&service, arguments);
    assert(service.adapter_commands.report_one_pending);
    assert(memcmp(service.adapter_commands.report_one, arguments + 1,
                  WHEEL_OUTPUT_REPORT_ONE_SIZE) == 0);
}

static void test_routes_packed_report_commands(void) {
    WheelService service;
    initialize_service(&service);
    UsbOperatingModeCommand command = {.opcode = 0x0a, .parameters = {7, 0, 0, 0}};

    assert(wheel_service_apply_packed_report_command(&service, &command));
    assert(service.protocol.output_reports.report_two[0] == 0xff);
    assert(service.protocol.output_reports.report_two[1] == 0xff);
    assert(service.adapter_commands.report_two_pending);
    assert(memcmp(service.adapter_commands.report_two, service.protocol.output_reports.report_two,
                  WHEEL_OUTPUT_REPORT_TWO_SIZE) == 0);

    initialize_service(&service);
    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    assert(wheel_service_apply_packed_report_command(&service, &command));
    assert(!service.adapter_commands.report_two_pending);
    command.opcode = 0x0b;
    assert(wheel_service_apply_packed_report_command(&service, &command));
    assert(service.adapter_commands.report_one_pending);
    assert(service.protocol.output_reports.report_one[0] == 0xff);
    assert(service.protocol.output_reports.report_one[1] == 0xff);

    command.opcode = 0x0c;
    assert(!wheel_service_apply_packed_report_command(&service, &command));
    assert(!wheel_service_apply_packed_report_command(NULL, &command));
    assert(!wheel_service_apply_packed_report_command(&service, NULL));
}

static void test_routes_auxiliary_output_commands(void) {
    WheelService service;
    initialize_service(&service);
    UsbOperatingModeCommand command = {
        .opcode = WHEEL_AUXILIARY_OPTION_OPCODE,
        .parameters = {2, 0, 0, 0},
    };

    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 2);
    assert(!service.protocol.alternate_output.suppress_auxiliary_display);

    command.parameters[0] = 1;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 1);
    assert(service.protocol.alternate_output.suppress_auxiliary_display);

    command.parameters[0] = 0;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 0);
    assert(!service.protocol.alternate_output.suppress_auxiliary_display);

    command.opcode = WHEEL_AUXILIARY_CODE_MODE_OPCODE;
    command.parameters[0] = UINT8_MAX;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.code_mode);

    command.opcode = WHEEL_AUXILIARY_REPORT_OPCODE;
    command.parameters[0] = 0x01;
    command.parameters[1] = 0x34;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.report == 0x0134);
    assert(service.protocol.mode_one_output.vibration[0] == 0x34);
    assert(service.protocol.mode_one_output.vibration[1] == 0x01);
    assert(service.protocol.alternate_output.display.auxiliary == 0x34);
    assert(service.protocol.alternate_output.auxiliary_status);
    assert(service.protocol.adapter_output.display_report == 0x0134);

    command.opcode = 0x09;
    assert(!wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(!wheel_service_apply_auxiliary_output_command(NULL, &command));
    assert(!wheel_service_apply_auxiliary_output_command(&service, NULL));
}

static void test_routes_report_six_command(void) {
    WheelService service;
    initialize_service(&service);
    UsbOperatingModeCommand command = {.opcode = 0x0d, .parameters = {0xa5, 0x12, 0x34, 0x5a}};

    assert(wheel_service_apply_report_six_command(&service, &command));
    assert(service.protocol.output_reports.report_four[0] == 0xa5);
    assert(service.protocol.output_reports.report_four[1] == 0x5a);
    assert(service.adapter_commands.report_four[0] == 0xa5);
    assert(service.adapter_commands.report_four[1] == 0x5a);
    assert(service.adapter_commands.report_six_pending);

    command.opcode = 0x0c;
    assert(!wheel_service_apply_report_six_command(&service, &command));
    assert(!wheel_service_apply_report_six_command(NULL, &command));
    assert(!wheel_service_apply_report_six_command(&service, NULL));
}

static void test_routes_and_toggles_interface_mode_gate(void) {
    WheelService service;
    initialize_service(&service);
    UsbOperatingModeCommand command = {.opcode = 0x0e, .parameters = {2}};

    assert(wheel_service_apply_interface_mode_command(&service, &command));
    assert(wheel_output_reports_interface_mode_gate(&service.protocol.output_reports));
    command.parameters[0] = 0;
    assert(wheel_service_apply_interface_mode_command(&service, &command));
    assert(!wheel_output_reports_interface_mode_gate(&service.protocol.output_reports));

    service.protocol.request_ready = true;
    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    service.protocol.request[1] = 0;
    service.protocol.request[2] = 0x90;
    wheel_service_update_interface_mode_gate(&service, 1);
    assert(wheel_output_reports_interface_mode_gate(&service.protocol.output_reports));

    service.protocol.mode = 1;
    service.protocol.request[1] = 0;
    wheel_service_update_interface_mode_gate(&service, 202);
    service.protocol.request[1] = 0x90;
    wheel_service_update_interface_mode_gate(&service, 203);
    assert(wheel_output_reports_interface_mode_gate(&service.protocol.output_reports));

    command.opcode = 0x0f;
    assert(!wheel_service_apply_interface_mode_command(&service, &command));
    assert(!wheel_service_apply_interface_mode_command(NULL, &command));
    assert(!wheel_service_apply_interface_mode_command(&service, NULL));
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

    service.protocol.adapter.connected = true;
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

static void test_gates_torque_key_acknowledgement(void) {
    WheelService service;
    initialize_service(&service);

    assert(wheel_service_torque_key_acknowledgement_available(&service));

    service.protocol.capabilities.calibration_available = true;
    assert(!wheel_service_torque_key_acknowledgement_available(&service));

    service.protocol.capabilities.calibration_available = false;
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
    assert(!wheel_service_torque_key_acknowledgement_available(&service));

    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_SECONDARY;
    assert(!wheel_service_torque_key_acknowledgement_available(&service));

    service.protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    assert(wheel_service_torque_key_acknowledgement_available(&service));
}

static void test_reports_tuning_display_support(void) {
    WheelService service = {0};
    const uint8_t direct_modes[] = {9, 10, 11, 14, 15, 16, 23, 27, 28, 29};
    const uint8_t adapter_modes[] = {4, 6, 12, 21};

    for (uint8_t index = 0; index < sizeof(direct_modes); index++) {
        service.protocol.mode = direct_modes[index];
        assert(wheel_service_tuning_display_supported(&service));
    }
    for (uint8_t index = 0; index < sizeof(adapter_modes); index++) {
        service.protocol.mode = adapter_modes[index];
        assert(!wheel_service_tuning_display_supported(&service));
        service.protocol.adapter.connected = true;
        service.protocol.adapter.mode = 1;
        assert(wheel_service_tuning_display_supported(&service));
        service.protocol.adapter.mode = 0;
        assert(!wheel_service_tuning_display_supported(&service));
        service.protocol.adapter.connected = false;
    }
    service.protocol.mode = 24;
    assert(!wheel_service_tuning_display_supported(&service));
}

static void test_routes_tuning_display_output_by_connection(void) {
    WheelService service = {0};
    uint8_t frame[33] = {0};
    const uint8_t text[] = {'B', 'A', 'S', 'E'};

    service.protocol.mode = 16;
    assert(wheel_service_queue_tuning_display_command(&service, 0x0a));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[0] == 0xa6);
    assert(frame[1] == 0x82);
    assert(frame[2] == 0x0a);

    service.protocol.mode = 4;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    assert(!wheel_service_queue_tuning_display_command(&service, 0x0a));
    assert(wheel_service_queue_adapter_text_line(&service, 1, 0x10, text, sizeof(text)));
    assert(service.adapter_commands.text_lines_pending == 1);
    assert(wheel_service_queue_adapter_text_close(&service));
    assert(service.adapter_commands.text_close_pending);
}

static void test_activates_interface_presentation_by_connection(void) {
    WheelService service = {0};
    uint8_t frame[33] = {0};

    service.protocol.mode = 16;
    assert(wheel_service_activate_interface_presentation(&service, 2));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[1] == 0x21);

    service = (WheelService){0};
    service.protocol.mode = 4;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    assert(wheel_service_activate_interface_presentation(&service, 1));
    assert(service.adapter_commands.interface_presentation_pending);
    assert(service.adapter_commands.interface_presentation_offset == 0x20);

    service = (WheelService){0};
    service.protocol.mode = 16;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    assert(wheel_service_activate_interface_presentation(&service, 4));
    assert(!service.adapter_commands.interface_presentation_pending);
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[1] == 0x80);

    service = (WheelService){0};
    service.protocol.mode = 16;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    assert(wheel_service_activate_interface_presentation(&service, 5));
    assert(!service.adapter_commands.interface_presentation_pending);
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[0] == 0xa6);
    assert(frame[1] == 0x81);

    service.protocol.mode = 4;
    service.protocol.adapter.connected = false;
    assert(!wheel_service_activate_interface_presentation(&service, 1));
    assert(!wheel_service_activate_interface_presentation(NULL, 1));
}

static void test_selects_calibration_advance_button_by_wheel_mode(void) {
    WheelService service;
    initialize_service(&service);

    service.protocol.mode = 1;
    service.protocol.request_ready = true;
    service.protocol.mode_one_input.buttons[1] = 0x80;
    assert(wheel_service_calibration_advance_input_active(&service));
    service.protocol.mode_one_input.buttons[1] = 0;
    assert(!wheel_service_calibration_advance_input_active(&service));

    service.protocol.request_ready = false;
    service.protocol.mode = WHEEL_MODE_REMOTE_TUNING_LEGACY;
    service.button_banks[1] = 0x80;
    assert(!wheel_service_calibration_advance_input_active(&service));
    service.button_banks[2] = 0x01;
    assert(wheel_service_calibration_advance_input_active(&service));

    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    assert(wheel_service_calibration_advance_input_active(&service));
    service.protocol.mode = WHEEL_MODE_LEGACY_COMPATIBILITY;
    assert(wheel_service_calibration_advance_input_active(&service));

    service.protocol.mode = WHEEL_MODE_REMOTE_TUNING_EXTENDED;
    assert(wheel_service_calibration_advance_input_active(&service));
    service.button_banks[1] = 0;
    assert(!wheel_service_calibration_advance_input_active(&service));
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

static void test_reports_bite_point_adjustment(void) {
    WheelService service;
    initialize_service(&service);
    uint8_t percent = 0;

    assert(!wheel_service_bite_point_adjustment(&service, &percent));
    service.protocol.axis_override_processor.paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_ADJUSTING;
    service.protocol.paddle_bite_point_percent = 62;
    assert(wheel_service_bite_point_adjustment(&service, &percent));
    assert(percent == 62);
}

static void test_applies_vibration_to_every_packet_family(void) {
    WheelService service;
    initialize_service(&service);
    const WheelVibrationOutput output = {.channels = {0x34, 0x56}};

    wheel_service_set_vibration_output(&service, &output);

    assert(service.protocol.mode_one_output.vibration[0] == 0x34);
    assert(service.protocol.mode_one_output.vibration[1] == 0x56);
    assert(service.protocol.mode_four_output.vibration[0] == 0x34);
    assert(service.protocol.mode_four_output.vibration[1] == 0x56);
    assert(service.protocol.crc_output.vibration[0] == 0x34);
    assert(service.protocol.crc_output.vibration[1] == 0x56);
    assert(service.auxiliary_output.report == 0x5634);
    assert(service.protocol.alternate_output.display.auxiliary == 0x34);
    assert(!service.protocol.alternate_output.auxiliary_status);
    assert(service.protocol.adapter_output.display_report == 0x5634);

    WheelDisplayOutput display = {.glyphs = {1, 2, 3}};
    wheel_service_set_display_output(&service, &display);
    assert(service.protocol.alternate_output.display.auxiliary == 0x34);
}

static void test_applies_legacy_axes_to_every_packet_family(void) {
    WheelService service;
    initialize_service(&service);
    const uint8_t axes[2] = {0x34, 0x56};

    wheel_service_set_legacy_axes(&service, axes);

    assert(service.protocol.mode_one_output.legacy_axes[0] == 0x34);
    assert(service.protocol.mode_one_output.legacy_axes[1] == 0x56);
    assert(service.protocol.mode_four_output.legacy_axes[0] == 0x34);
    assert(service.protocol.mode_four_output.legacy_axes[1] == 0x56);
    assert(service.protocol.crc_output.legacy_axes[0] == 0x34);
    assert(service.protocol.crc_output.legacy_axes[1] == 0x56);
}

static void test_resets_host_protocol_outputs(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.mode = 10;
    service.protocol.adapter = (WheelAdapterInput){.mode = 1, .connected = true};
    const WheelVibrationOutput vibration = {.channels = {0x34, 0x56}};
    const uint8_t axes[2] = {0x78, 0x9a};
    const uint8_t packed[4] = {0xff, 0xff, 0xff, 0xff};
    const WheelDisplayOutput display = {.glyphs = {1, 2, 3}, .third_glyph_marker = true};
    wheel_service_set_vibration_output(&service, &vibration);
    wheel_service_set_legacy_axes(&service, axes);
    wheel_service_set_display_output(&service, &display);
    assert(wheel_output_reports_queue_packed(&service.protocol.output_reports, 2, packed,
                                             service.protocol.mode));
    assert(wheel_output_reports_queue_packed(&service.protocol.output_reports, 1, packed,
                                             service.protocol.mode));

    wheel_service_reset_host_protocol_outputs(&service);

    assert(service.auxiliary_output.report == 0);
    assert(service.protocol.adapter_output.display_report == 0);
    assert(service.protocol.mode_one_output.legacy_axes[0] == 0);
    assert(service.protocol.mode_one_output.legacy_axes[1] == 0);
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_ONE_SIZE; index++) {
        assert(service.protocol.output_reports.report_one[index] == 0);
    }
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_TWO_SIZE; index++) {
        assert(service.protocol.output_reports.report_two[index] == 0);
    }
    assert(service.adapter_commands.report_one_pending);
    assert(service.adapter_commands.report_two_pending);
    assert(service.display_output.glyphs[0] == 1);
    assert(service.display_output.glyphs[1] == 2);
    assert(service.display_output.glyphs[2] == 3);
    assert(service.display_output.third_glyph_marker);

    service.protocol.mode = 1;
    service.protocol.adapter.connected = false;
    wheel_service_reset_host_protocol_outputs(&service);
    assert(service.display_output.glyphs[0] == 0);
    assert(service.display_output.glyphs[1] == 0);
    assert(service.display_output.glyphs[2] == 0);
    assert(!service.display_output.third_glyph_marker);
}

static void test_preserves_default_display_behind_temporary_overlay(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.mode = 1;
    const WheelDisplayOutput first = {
        .glyphs = {1, 2, 3},
        .auxiliary = 0x45,
        .third_glyph_marker = true,
    };
    wheel_service_set_display_output(&service, &first);

    wheel_service_begin_display_overlay(&service, 0x93, 100);
    assert(wheel_service_display_overlay_active(&service));
    assert(service.display_output.glyphs[0] == 0);
    assert(service.display_output.glyphs[1] == 0x78);
    assert(service.display_output.glyphs[2] == 0);
    assert(service.display_output.auxiliary == 0x45);
    assert(!service.display_output.third_glyph_marker);

    WheelDisplayOutput *default_output = wheel_service_default_display_output(&service);
    *default_output = (WheelDisplayOutput){
        .glyphs = {4, 5, 6},
        .auxiliary = 0x67,
        .third_glyph_marker = true,
    };
    wheel_service_set_display_output(&service, default_output);
    wheel_service_reset_host_protocol_outputs(&service);
    assert(service.display_output.glyphs[1] == 0x78);
    assert(service.default_display_output.glyphs[0] == 4);
    assert(service.default_display_output.glyphs[1] == 5);
    assert(service.default_display_output.glyphs[2] == 6);

    assert(!wheel_service_update_display_overlay(&service, 2099));
    assert(wheel_service_update_display_overlay(&service, 2100));
    assert(!wheel_service_display_overlay_active(&service));
    assert(service.display_output.glyphs[0] == 4);
    assert(service.display_output.glyphs[1] == 5);
    assert(service.display_output.glyphs[2] == 6);
    assert(service.display_output.auxiliary == 0x67);
    assert(service.display_output.third_glyph_marker);
}

static void test_prioritizes_interaction_display_override(void) {
    WheelService service;
    initialize_service(&service);
    const WheelDisplayOutput first = {
        .glyphs = {1, 2, 3},
        .auxiliary = 0x45,
        .third_glyph_marker = true,
    };
    const WheelDisplayOutput override = {
        .glyphs = {7, 8, 9},
        .auxiliary = 0x23,
    };
    const WheelDisplayOutput second = {
        .glyphs = {4, 5, 6},
        .auxiliary = 0x67,
        .third_glyph_marker = true,
    };
    wheel_service_set_display_output(&service, &first);
    wheel_service_begin_display_overlay(&service, 0x93, 100);
    wheel_service_set_display_override(&service, &override);
    assert(service.display_override_active);
    assert(service.display_output.glyphs[0] == 7);
    assert(service.display_output.glyphs[1] == 8);
    assert(service.display_output.glyphs[2] == 9);
    assert(service.display_output.auxiliary == 0x23);

    wheel_service_set_display_output(&service, &second);
    wheel_service_clear_display_override(&service);
    assert(!service.display_override_active);
    assert(service.display_output.glyphs[1] == 0x78);
    assert(service.display_output.auxiliary == 0x67);

    wheel_service_set_display_override(&service, &override);
    assert(!wheel_service_update_display_overlay(&service, 2100));
    assert(service.display_output.glyphs[0] == 7);
    wheel_service_clear_display_override(&service);
    assert(service.display_output.glyphs[0] == 4);
    assert(service.display_output.glyphs[1] == 5);
    assert(service.display_output.glyphs[2] == 6);
    assert(service.display_output.auxiliary == 0x67);
    assert(service.display_output.third_glyph_marker);
}

static void test_reports_host_capability_recovery_inputs(void) {
    WheelService service;
    initialize_service(&service);

    service.protocol.capabilities.capability_flags = 0x0b00;
    assert(wheel_service_capability_flags(&service) == 0x0b00);
    assert(!wheel_service_host_capability_enabled(&service));
    wheel_service_set_host_capability(&service, true);
    assert(wheel_service_host_capability_enabled(&service));

    service.protocol.adapter.buttons[1] = 0x80;
    assert(!wheel_service_adapter_requests_host_capability(&service));
    service.protocol.adapter.connected = true;
    assert(wheel_service_adapter_requests_host_capability(&service));
    service.protocol.adapter.buttons[1] = 0x7f;
    assert(!wheel_service_adapter_requests_host_capability(&service));
}

static void test_reports_force_output_readiness(void) {
    WheelService service;
    initialize_service(&service);

    service.protocol.phase = WHEEL_PROTOCOL_WAITING;
    assert(!wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_SELECTING;
    assert(!wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_UNSUPPORTED;
    assert(!wheel_service_force_output_ready(&service));
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_AUTHENTICATING;
    assert(wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    assert(wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
    assert(wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_SECONDARY;
    assert(wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
}

static void test_exposes_playstation_wheel_inputs(void) {
    WheelService service;
    initialize_service(&service);

    service.protocol.request_ready = true;
    service.protocol.mode = 4;
    service.protocol.request[0] = 0x81;
    service.protocol.request[1] = 0x23;
    service.protocol.request[2] = 0x45;
    service.protocol.request[3] = 0x67;
    service.protocol.request[4] = 0x89;
    service.protocol.request[5] = 0xf4;
    service.protocol.request[22] = 0xab;
    service.protocol.request[23] = 0xcd;
    service.protocol.request[24] = 0xef;
    service.protocol.mode_four_input.axis_report_enabled = 1;
    service.protocol.adapter = (WheelAdapterInput){
        .buttons = {0x12, 0x34, 0x56},
        .axes = {0x78, 0x9a},
        .mode = 1,
        .connected = true,
    };

    assert(wheel_service_axis_report_enabled(&service));
    WheelInputSnapshot snapshot;
    assert(wheel_service_input_snapshot(&service, &snapshot));
    assert(snapshot.directional_buttons == 0x81);
    assert(snapshot.secondary_buttons == 0x4523);
    assert(snapshot.clutch_paddles[0] == 0x67);
    assert(snapshot.clutch_paddles[1] == 0x89);
    assert(snapshot.tuning_input == -12);
    assert(snapshot.auxiliary_report[0] == 0xab);
    assert(snapshot.auxiliary_report[1] == 0xcd);
    assert(snapshot.auxiliary_report[2] == 0xef);
    assert(snapshot.axis_report_enabled);
    assert(wheel_service_adapter(&service) == &service.protocol.adapter);
    assert(wheel_service_adapter(&service)->buttons[2] == 0x56);
    assert(wheel_service_adapter(&service)->axes[1] == 0x9a);
    assert(wheel_service_adapter(&service)->mode == 1);
    assert(wheel_service_adapter(&service)->connected);

    service.protocol.request_ready = false;
    assert(!wheel_service_input_snapshot(&service, &snapshot));
    assert(snapshot.directional_buttons == 0);
    assert(snapshot.secondary_buttons == 0);
    assert(snapshot.tuning_input == 0);
    assert(!snapshot.axis_report_enabled);
}

static void test_rejects_invalid_service_requests(void) {
    WheelService service;
    WheelInputSnapshot snapshot;
    WheelMultiPositionInput multi_position;
    WheelDisplayOutput display = {0};
    UsbOperatingModeCommand command = {0};
    uint8_t host_controls[WHEEL_ADAPTER_HOST_CONTROLS_SIZE];
    uint8_t legacy_axes[2] = {0};
    uint8_t telemetry[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE] = {0};
    uint8_t text = 0;
    CommandTransport adapter_transport;
    initialize_service(&service);
    command_transport_init(&adapter_transport);

    wheel_service_reset_adapter_commands(NULL);
    wheel_service_run_adapter_commands(NULL, &adapter_transport);
    wheel_service_run_adapter_commands(&service, NULL);
    assert(!wheel_service_take_adapter_host_controls(NULL, host_controls));
    assert(!wheel_service_take_adapter_host_controls(&service, NULL));
    wheel_service_queue_adapter_remote_tuning_active(NULL, true);
    wheel_service_queue_adapter_refresh_state(NULL, true);
    wheel_service_queue_adapter_setup_selection(NULL, 1);
    wheel_service_queue_adapter_display_state(NULL, 1);
    wheel_service_queue_adapter_display_state(&service, 0);
    assert(!wheel_service_queue_tuning_display_command(NULL, 1));
    assert(!wheel_service_queue_tuning_display_command(&service, 1));
    assert(!wheel_service_queue_adapter_text_line(NULL, 1, 0, &text, 1));
    assert(!wheel_service_queue_adapter_text_line(&service, 1, 0, &text, 1));
    assert(!wheel_service_queue_adapter_text_close(NULL));
    assert(!wheel_service_queue_adapter_text_close(&service));
    wheel_service_set_auxiliary_report(NULL, 1);
    wheel_service_set_display_override(NULL, &display);
    wheel_service_set_display_override(&service, NULL);
    wheel_service_clear_display_override(NULL);
    wheel_service_clear_display_override(&service);
    wheel_service_set_auxiliary_output_option(NULL, 1);
    wheel_service_set_legacy_axes(NULL, legacy_axes);
    wheel_service_set_legacy_axes(&service, NULL);
    wheel_service_reset_host_protocol_outputs(NULL);
    assert(!wheel_service_remote_tuning_response_pending(NULL));
    assert(!wheel_service_apply_auxiliary_output_command(NULL, &command));
    assert(!wheel_service_apply_auxiliary_output_command(&service, NULL));
    command.opcode = UINT8_MAX;
    assert(!wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(!wheel_service_apply_multi_position_command(NULL, &command));
    assert(!wheel_service_apply_packed_report_command(NULL, &command));
    assert(!wheel_service_apply_packed_report_command(&service, NULL));
    assert(!wheel_service_apply_packed_report_command(&service, &command));
    assert(!wheel_service_apply_report_six_command(NULL, &command));
    assert(!wheel_service_apply_report_six_command(&service, NULL));
    assert(!wheel_service_apply_report_six_command(&service, &command));
    assert(!wheel_service_apply_interface_mode_command(NULL, &command));
    assert(!wheel_service_apply_interface_mode_command(&service, NULL));
    assert(!wheel_service_apply_interface_mode_command(&service, &command));
    wheel_service_update_interface_mode_gate(NULL, 0);
    wheel_service_update_interface_mode_gate(&service, 0);
    assert(wheel_service_multi_position_mode(NULL, TUNING_MULTI_POSITION_AUTOMATIC) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(!wheel_service_multi_position_supported(NULL));
    assert(!wheel_service_multi_position_input(NULL, 0, &multi_position));
    assert(!wheel_service_multi_position_input(&service, 0, NULL));
    assert(!wheel_service_multi_position_input(&service, 0, &multi_position));
    wheel_service_apply_output_report(NULL, telemetry);
    wheel_service_apply_output_report(&service, NULL);
    assert(!wheel_service_queue_remote_telemetry(NULL, telemetry));
    assert(!wheel_service_remote_telemetry_pending(NULL));
    assert(!wheel_service_start_protocol_exchange(NULL, 0));
    service.transport = NULL;
    assert(!wheel_service_start_protocol_exchange(&service, 0));
    service.transport = &transport;
    service.transport->status = SERIAL_SERVICE_PENDING;
    assert(!wheel_service_start_protocol_exchange(&service, 0));
    assert(!wheel_service_take_protocol_exchange_completed(NULL));
    assert(!wheel_service_take_protocol_exchange_completed(&service));
    assert(!wheel_service_input_snapshot(&service, NULL));
    assert(!wheel_service_input_snapshot(&service, &snapshot));
}

static void prepare_alternative_shifter_input(WheelService *service, uint8_t mode,
                                              uint8_t latch_flags) {
    initialize_service(service);
    service->protocol.request_ready = true;
    service->protocol.mode = mode;
    service->protocol.request[1] = 0x09;
    if (mode == WHEEL_MODE_LEGACY_ALTERNATE) {
        service->protocol.packed_input.axis_report_enabled = 1;
    } else {
        service->protocol.mode_one_input.axis_report_enabled = 1;
        service->protocol.mode_one_input.controls.latch_flags = latch_flags;
    }
}

static void test_toggles_alternative_shifter_mode(void) {
    WheelService service;
    prepare_alternative_shifter_input(&service, 1, 2);

    assert(wheel_service_update_alternative_shifter(&service, true, 1) ==
           WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED);
    assert(!wheel_service_alternative_shifter_enabled(&service));

    service.protocol.mode_one_input.controls.latch_flags = 3;
    assert(wheel_service_update_alternative_shifter(&service, true, 1) ==
           WHEEL_ALTERNATIVE_SHIFTER_ENABLED);
    assert(wheel_service_alternative_shifter_enabled(&service));
    assert(service.protocol.button_latch_enabled);
    assert(service.protocol.profile_transition_pending);

    assert(wheel_service_update_alternative_shifter(&service, true, 2) ==
           WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED);
    service.protocol.request[1] = 0;
    assert(wheel_service_update_alternative_shifter(&service, true, 3) ==
           WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED);
    service.protocol.request[1] = 0x09;
    assert(wheel_service_update_alternative_shifter(&service, true, 801) ==
           WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED);
    assert(wheel_service_update_alternative_shifter(&service, true, 802) ==
           WHEEL_ALTERNATIVE_SHIFTER_DISABLED);
    assert(!wheel_service_alternative_shifter_enabled(&service));
    assert(!service.protocol.button_latch_enabled);
}

static void test_legacy_alternative_shifter_ignores_latch_flags(void) {
    WheelService service;
    prepare_alternative_shifter_input(&service, WHEEL_MODE_LEGACY_ALTERNATE, 0);

    assert(wheel_service_update_alternative_shifter(&service, false, 1) ==
           WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED);
    assert(wheel_service_update_alternative_shifter(&service, true, 1) ==
           WHEEL_ALTERNATIVE_SHIFTER_ENABLED);

    service.protocol.packed_input.axis_report_enabled = 0;
    assert(wheel_service_update_alternative_shifter(&service, true, 2) ==
           WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED);
    assert(!wheel_service_alternative_shifter_enabled(&service));
    assert(!service.protocol.button_latch_enabled);
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
    test_forces_and_reports_protocol_exchange();
    test_initializes_rotary_input();
    test_routes_multi_position_mode();
    test_builds_direct_multi_position_input();
    test_builds_adapter_multi_position_input();
    test_marks_extended_multi_position_layout();
    test_filters_adapter_remote_tuning_active_state();
    test_retains_adapter_display_state_across_command_resets();
    test_mirrors_standard_adapter_output_reports();
    test_mirrors_extended_adapter_output_reports();
    test_routes_auxiliary_output_commands();
    test_routes_packed_report_commands();
    test_routes_report_six_command();
    test_routes_and_toggles_interface_mode_gate();
    test_rejects_unavailable_multi_position_input();
    test_selects_extended_report_fields();
    test_reports_calibration_availability();
    test_gates_torque_key_acknowledgement();
    test_reports_tuning_display_support();
    test_routes_tuning_display_output_by_connection();
    test_activates_interface_presentation_by_connection();
    test_selects_calibration_advance_button_by_wheel_mode();
    test_reports_mode_gated_input_capability();
    test_exposes_axis_overrides();
    test_reports_bite_point_adjustment();
    test_applies_vibration_to_every_packet_family();
    test_applies_legacy_axes_to_every_packet_family();
    test_resets_host_protocol_outputs();
    test_preserves_default_display_behind_temporary_overlay();
    test_prioritizes_interaction_display_override();
    test_reports_host_capability_recovery_inputs();
    test_reports_force_output_readiness();
    test_exposes_playstation_wheel_inputs();
    test_rejects_invalid_service_requests();
    test_toggles_alternative_shifter_mode();
    test_legacy_alternative_shifter_ignores_latch_flags();
    return 0;
}
