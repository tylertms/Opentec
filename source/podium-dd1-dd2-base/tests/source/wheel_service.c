#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/wheel_link.h"
#include "wheel/display_output.h"
#include "wheel/protocol.h"
#include "wheel/service.h"
#include "wheel/transport_frame.h"

static uint8_t transmitted[WHEEL_TRANSPORT_FRAME_SIZE];
static uint8_t received[WHEEL_TRANSPORT_FRAME_SIZE];
static bool received_ready;

enum {
    WHEEL_BUTTON_PRIMARY_RESPONSE = 0xe0,
    WHEEL_BUTTON_SECONDARY_RESPONSE = 0xc0,
};

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

static void respond_active(uint8_t flags) {
    WheelTransportFrame frame = {
        .command = 2,
        .length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    frame.data[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    frame.data[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
    frame.data[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(frame.data);
    respond_frame(&frame);
}

static void respond_active_buttons(uint8_t first, uint8_t second, uint8_t third) {
    WheelTransportFrame frame = {
        .command = 2,
        .length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    frame.data[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    frame.data[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    frame.data[2] = first;
    frame.data[3] = second;
    frame.data[4] = third;
    frame.data[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(frame.data);
    respond_frame(&frame);
}

static uint32_t begin_scan_mode(WheelService *service, uint8_t command) {
    wheel_service_init(service);
    wheel_service_run(service, 0);
    assert(request().command == 2);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        wheel_service_run(service, now_ms);
    }
    WheelTransportFrame frame = request();
    assert(frame.data[WHEEL_PROTOCOL_FLAGS_OFFSET] == WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);
    respond_protocol(command, 0);
    wheel_service_run(service, 4);
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
        WheelTransportFrame scan = request();
        while (scan.data[0] != mapping->phase) {
            respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
            wheel_service_run(&service, now_ms++);
            scan = request();
        }
        respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | mapping->sample);
        wheel_service_run(&service, now_ms++);
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
        WheelTransportFrame scan = request();
        while (scan.data[0] != WHEEL_SCAN_PHASE_FIRST) {
            respond_scan(WHEEL_BUTTON_SECONDARY_RESPONSE);
            wheel_service_run(&service, now_ms++);
            scan = request();
        }
        respond_scan(WHEEL_BUTTON_SECONDARY_RESPONSE | 0x02);
        wheel_service_run(&service, now_ms++);
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

    WheelTransportFrame scan = request();
    assert(scan.command == 3);
    assert(scan.length == WHEEL_TRANSPORT_PAYLOAD_SIZE);
    assert(scan.data[0] == 8);
    assert(scan.data[1] == UINT8_MAX);
    assert(scan.data[WHEEL_TRANSPORT_PAYLOAD_SIZE - 1] == 1);

    for (uint8_t cycle = 0; cycle < WHEEL_SCAN_SAMPLE_DEPTH; cycle++) {
        for (uint8_t phase = 0; phase < 4; phase++) {
            respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | 0x1f);
            wheel_service_run(&service, now_ms++);
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
        WheelTransportFrame scan = request();
        while (scan.data[0] != WHEEL_SCAN_PHASE_AUXILIARY) {
            respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
            wheel_service_run(&service, now_ms++);
            scan = request();
        }
        respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | 0x01);
        wheel_service_run(&service, now_ms++);
    }
    assert((wheel_service_buttons(&service)[2] & 0x04) != 0);

    WheelTransportFrame scan = request();
    while (scan.data[0] != WHEEL_SCAN_PHASE_AUXILIARY) {
        respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
        wheel_service_run(&service, now_ms++);
        scan = request();
    }
    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    wheel_service_run(&service, now_ms);
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
    wheel_service_run(&service, now_ms++);
    WheelTransportFrame scan = request();
    assert(scan.data[1] == (uint8_t)~0x0a);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    wheel_service_run(&service, now_ms++);
    scan = request();
    assert(scan.data[1] == (uint8_t)~0x36);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    wheel_service_run(&service, now_ms++);
    scan = request();
    assert(scan.data[1] == (uint8_t)~0xc9);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    wheel_service_run(&service, now_ms);
    scan = request();
    assert(scan.data[1] == (uint8_t)~0x37);
}

static void test_keeps_protocol_transport_for_packet_modes(void) {
    WheelService service;
    received_ready = false;
    wheel_service_init(&service);
    wheel_service_run(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        wheel_service_run(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    wheel_service_run(&service, 4);
    WheelTransportFrame active = {
        .command = 2,
        .length = WHEEL_PROTOCOL_PACKET_SIZE,
    };
    active.data[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    active.data[31] = 0x73;
    active.data[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    active.data[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(active.data);
    respond_frame(&active);
    wheel_service_run(&service, 5);

    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_service_mode(&service) == 1);
    assert(wheel_service_axis_limit(&service) == 0x73);
    WheelTransportFrame frame = request();
    assert(frame.command == 2);
    assert(frame.length == WHEEL_PROTOCOL_PACKET_SIZE);
    assert(frame.data[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_message_valid(frame.data));
}

static void test_publishes_packet_mode_buttons(void) {
    WheelService service;
    received_ready = false;
    wheel_service_init(&service);
    wheel_service_run(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        wheel_service_run(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    wheel_service_run(&service, 4);

    for (uint32_t now_ms = 5; now_ms <= 7; now_ms++) {
        respond_active_buttons(0x20, 0x04, 0x02);
        wheel_service_run(&service, now_ms);
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
    wheel_service_init(&service);
    wheel_service_run(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        wheel_service_run(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    wheel_service_run(&service, 4);
    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    wheel_service_run(&service, 5);

    respond_active(0);
    wheel_service_run(&service, 2004);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    wheel_service_run(&service, 2005);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
    assert(request().command == 2);
}

static void test_ready_packet_refreshes_activity_at_deadline(void) {
    WheelService service;
    received_ready = false;
    wheel_service_init(&service);
    wheel_service_run(&service, 0);
    for (uint32_t now_ms = 1; now_ms <= 3; now_ms++) {
        respond_protocol(0, 0);
        wheel_service_run(&service, now_ms);
    }
    respond_protocol(WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    wheel_service_run(&service, 4);
    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    wheel_service_run(&service, 5);

    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    wheel_service_run(&service, 2005);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    wheel_service_run(&service, 4004);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    wheel_service_run(&service, 4005);
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
    wheel_protocol_set_axis_processing(&service.protocol, 7, WHEEL_AXIS_OVERRIDE_MODE_PRIMARY,
                                       0x3c);
    service.protocol.axis_override_processor.multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
    service.protocol.axis_override_processor.x_available = true;
    service.protocol.axis_override_processor.y_available = true;
    service.protocol.axis_override_processor.overrides.axis_5.enabled = true;
    service.protocol.axis_override_processor.overrides.axis_5.value = 0x7d;
    wheel_protocol_set_button_latch(&service.protocol, true, true);
    wheel_service_run(&service, now_ms + 10);

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
    assert(service.protocol.axis_calibration_value == 0x3c);
    assert(service.protocol.axis_override_processor.multiplex_phase == WHEEL_AXIS_MULTIPLEX_Y);
    assert(service.protocol.axis_override_processor.x_available);
    assert(service.protocol.axis_override_processor.y_available);
    assert(!service.protocol.axis_override_processor.overrides.axis_5.enabled);
    assert(service.protocol.axis_override_processor.overrides.axis_5.value == 0);
    assert(service.protocol.button_latch_enabled);
    assert(service.protocol.profile_transition_pending);
    assert(request().command == 2);
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
    return 0;
}
