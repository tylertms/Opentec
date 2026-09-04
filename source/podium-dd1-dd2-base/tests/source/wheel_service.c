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
static bool platform_start_successful = true;

enum {
    WHEEL_BUTTON_PRIMARY_RESPONSE = 0xe0,
    WHEEL_BUTTON_SECONDARY_RESPONSE = 0xc0,
    WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS = 2000,
    WHEEL_PROTOCOL_SELECTION_TIMEOUT_MS = 10000,
};

void platform_serial_link_init(void) {}

void platform_serial_link_reset(void) {}

bool platform_serial_link_start_periodic_recovery(void) { return true; }

bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    if (!platform_start_successful) {
        return false;
    }
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
    platform_start_successful = true;
    serial_service_init(&transport);
    wheel_service_init(service, &transport);
}

static void test_exposes_protocol_bridge_report_id(void) {
    WheelService service;
    initialize_service(&service);
    assert(wheel_service_protocol_bridge_report_id(&service) == 0x15);
    service.adapter_commands.endpoint_index = 1;
    assert(wheel_service_protocol_bridge_report_id(&service) == 0x16);
    service.adapter_commands.endpoint_index = 2;
    assert(wheel_service_protocol_bridge_report_id(&service) == 0);
    assert(wheel_service_protocol_bridge_report_id(NULL) == 0);
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

static void respond_scan_not_ready(void) {
    SerialPacket frame = {
        .type_flags = 3,
        .payload_length = SERIAL_PACKET_MAX_PAYLOAD_SIZE,
    };
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

static uint32_t begin_protocol_mode(WheelService *service, uint8_t command, uint8_t mode) {
    received_ready = false;
    initialize_service(service);
    run_service(service, 0);
    assert(request().type_flags == 2);

    respond_protocol(0, 0);
    run_service(service, 2);
    respond_protocol(0, 0);
    run_service(service, 13);
    respond_protocol(0, 0);
    run_service(service, 2014);
    assert(request().payload[WHEEL_PROTOCOL_FLAGS_OFFSET] == WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);
    respond_protocol(command, mode);
    run_service(service, 2016);
    return 2017;
}

static uint32_t begin_scan_mode(WheelService *service, uint8_t command) {
    return begin_protocol_mode(service, command, 0);
}

static void test_applies_scan_status_flags_once(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
    service.protocol.mode = WHEEL_MODE_SCAN_PRIMARY;
    service.protocol.crc_output.report_state = UINT8_MAX;
    service.protocol.crc_output.status_update_pending = true;

    wheel_service_run(&service, 0, true);
    SerialPacket scan = request();
    assert(scan.payload[0] == (WHEEL_SCAN_PHASE_AUXILIARY | 0x40 | 0x80));
    assert(!service.protocol.crc_output.status_update_pending);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, 1);
    scan = request();
    assert(scan.payload[0] == (WHEEL_SCAN_PHASE_THIRD | 0x40));
}

static void test_omits_scan_status_flags_when_available(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
    service.protocol.mode = WHEEL_MODE_SCAN_PRIMARY;

    wheel_service_run(&service, 0, true);

    assert(request().payload[0] == WHEEL_SCAN_PHASE_AUXILIARY);
}

static void test_retries_scan_after_transport_start_failure(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
    service.protocol.mode = WHEEL_MODE_SCAN_PRIMARY;
    service.protocol.crc_output.report_state = 0;
    service.protocol.crc_output.status_update_pending = true;
    service.scan_phase = WHEEL_SCAN_PHASE_SECOND;
    platform_start_successful = false;

    wheel_service_run(&service, 0, true);

    assert(service.protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(service.protocol.mode == WHEEL_MODE_SCAN_PRIMARY);
    assert(service.scan_phase == WHEEL_SCAN_PHASE_SECOND);
    assert(service.protocol.crc_output.status_update_pending);
    assert(service.request_kind == WHEEL_SERVICE_REQUEST_NONE);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    platform_start_successful = true;
    wheel_service_run(&service, 1, true);

    assert(service.scan_phase == WHEEL_SCAN_PHASE_FIRST);
    assert(service.request_kind == WHEEL_SERVICE_REQUEST_BUTTONS);
    assert(service.protocol.crc_output.status_update_pending == false);
    assert(request().payload[0] == (WHEEL_SCAN_PHASE_FIRST | 0x80));
    assert(transport.status == SERIAL_SERVICE_PENDING);
}

static void test_retries_protocol_after_transport_start_failure(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    service.protocol_transport_deadline_active = true;
    service.protocol_transport_deadline_ms = 0;
    platform_start_successful = false;

    wheel_service_run(&service, 1, true);

    assert(service.protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(service.protocol.mode == WHEEL_MODE_LEGACY_ALTERNATE);
    assert(service.request_kind == WHEEL_SERVICE_REQUEST_NONE);
    assert(service.protocol_transport_deadline_active);
    assert(service.protocol_transport_deadline_ms == 0);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    platform_start_successful = true;
    wheel_service_run(&service, 2, true);

    assert(service.request_kind == WHEEL_SERVICE_REQUEST_PROTOCOL);
    assert(transport.status == SERIAL_SERVICE_PENDING);
}

static void test_paces_logical_protocol_requests(void) {
    WheelService service;
    initialize_service(&service);

    wheel_service_run(&service, 0, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    assert(service.protocol_transport_interval_ms == 1);
    assert(service.protocol_transport_deadline_ms == 1);

    respond_protocol(0, 0);
    run_service(&service, 2);
    assert(service.protocol_transport_interval_ms == 10);
    assert(service.protocol_transport_deadline_ms == 12);

    serial_service_cancel(&transport);
    wheel_service_run(&service, 12, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    wheel_service_run(&service, 13, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);

    serial_service_cancel(&transport);
    service.protocol.phase = WHEEL_PROTOCOL_ACKNOWLEDGING;
    service.protocol_transport_deadline_active = false;
    wheel_service_run(&service, 20, true);
    assert(service.protocol_transport_interval_ms == 2000);
    assert(service.protocol_transport_deadline_ms == 2020);
}

static void test_recovers_malformed_protocol_response_and_clears_completion(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.display_character_mode = true;
    service.protocol.capabilities.report_flags = 0x1234;
    service.protocol.capabilities.capability_flags = 0x5678;
    service.protocol.capabilities.multi_position_override = 2;
    service.protocol.capabilities.calibration_available = true;
    service.protocol.capabilities.tuning_menu_available = true;
    service.protocol.capabilities.input_available = true;
    service.protocol.axis_override_processor.x_available = true;
    service.protocol.axis_override_processor.y_available = true;
    service.protocol.axis_override_processor.packet_axis_report_enabled = true;
    service.protocol_exchange_completed = true;
    service.auxiliary_output.exclusive_mode = true;
    service.auxiliary_output.latched_bands = 0x07;
    wheel_service_run(&service, 0, true);

    SerialPacket malformed = {
        .type_flags = 2,
        .payload_length = 1,
    };
    malformed.payload[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    respond_frame(&malformed);
    serial_service_run(&transport, 2);
    wheel_service_run(&service, 2, false);

    assert(service.protocol.phase == WHEEL_PROTOCOL_WAITING);
    assert(service.protocol.display_character_mode);
    assert(service.protocol.capabilities.report_flags == 0x1234);
    assert(service.protocol.capabilities.capability_flags == 0x5678);
    assert(service.protocol.capabilities.multi_position_override == 2);
    assert(!service.protocol.capabilities.calibration_available);
    assert(!service.protocol.capabilities.tuning_menu_available);
    assert(!service.protocol.capabilities.input_available);
    assert(service.protocol.axis_override_processor.x_available);
    assert(service.protocol.axis_override_processor.y_available);
    assert(service.protocol.axis_override_processor.packet_axis_report_enabled);
    assert(!service.protocol_exchange_completed);
    assert(!service.auxiliary_output.exclusive_mode);
    assert(service.auxiliary_output.latched_bands == 0);
    assert(service.request_kind == WHEEL_SERVICE_REQUEST_NONE);
    assert(service.protocol_transport_interval_ms == 1);
    assert(!service.protocol_transport_deadline_active);
    assert(memcmp(service.request, (uint8_t[sizeof(service.request)]){0},
                  sizeof(service.request)) == 0);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    wheel_service_run(&service, 2, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    assert(request().type_flags == 2);
}

static void test_resets_transient_display_state(void) {
    WheelService service;
    initialize_service(&service);
    const WheelDisplayOutput default_display = {
        .glyphs = {1, 2, 3},
        .auxiliary = 0x45,
        .third_glyph_marker = true,
    };
    const WheelDisplayOutput override = {
        .glyphs = {7, 8, 9},
        .auxiliary = 0x23,
    };

    wheel_service_set_display_output(&service, &default_display);
    wheel_service_begin_display_overlay(&service, 0x93, 100);
    wheel_service_set_display_override(&service, &override);
    assert(wheel_service_display_overlay_active(&service));
    assert(service.display_override_active);

    wheel_service_run(&service, 100, true);
    SerialPacket malformed = {
        .type_flags = 2,
        .payload_length = 1,
    };
    respond_frame(&malformed);
    serial_service_run(&transport, 102);
    wheel_service_run(&service, 102, false);

    assert(!wheel_service_display_overlay_active(&service));
    assert(!service.display_override_active);
    assert(memcmp(&service.display_override_output, &(WheelDisplayOutput){0},
                  sizeof(service.display_override_output)) == 0);
    assert(memcmp(&service.default_display_output, &default_display,
                  sizeof(service.default_display_output)) == 0);
    assert(memcmp(&service.display_output, &default_display, sizeof(service.display_output)) == 0);
    assert(memcmp(&service.protocol.mode_one_output.display, &default_display,
                  sizeof(default_display)) == 0);
    assert(memcmp(&service.protocol.mode_four_output.display, &default_display,
                  sizeof(default_display)) == 0);
    assert(memcmp(&service.protocol.crc_output.display, &default_display,
                  sizeof(default_display)) == 0);
    assert(memcmp(&service.protocol.adapter_output.display, &default_display,
                  sizeof(default_display)) == 0);
}

static void test_recovers_unknown_selection_after_deadline(void) {
    WheelService service;
    uint32_t now_ms = begin_protocol_mode(&service, 0x55, 0);
    assert(service.protocol.phase == WHEEL_PROTOCOL_SELECTING);
    assert(!service.bridge_recovery_pending);

    const uint32_t deadline_ms = 2014 + WHEEL_PROTOCOL_SELECTION_TIMEOUT_MS;
    for (now_ms += 1; now_ms < deadline_ms; now_ms += 2) {
        respond_protocol(0x55, 0);
        run_service(&service, now_ms);
    }
    respond_protocol(0x55, 0);
    run_service(&service, deadline_ms);

    assert(service.protocol.phase == WHEEL_PROTOCOL_SELECTING);
    assert(service.protocol_deadline_active == false);
    assert(wheel_service_bridge_recovery_pending(&service));
    assert(wheel_service_take_bridge_recovery(&service));
    assert(!wheel_service_bridge_recovery_pending(&service));
    assert(!wheel_service_take_bridge_recovery(&service));
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

static void test_delegates_protocol_button_families_and_scan_fallback(void) {
    WheelService service = {0};
    service.protocol.request_ready = true;

    service.protocol.mode = 1;
    assert(wheel_service_buttons(&service) == service.protocol.mode_one_input.buttons);
    service.protocol.mode = 4;
    assert(wheel_service_buttons(&service) == service.protocol.mode_four_input.buttons);
    service.protocol.mode = 0x10;
    assert(wheel_service_buttons(&service) == service.protocol.display_input.buttons);
    service.protocol.mode = 0x11;
    assert(wheel_service_buttons(&service) == service.protocol.remapped_input.buttons);
    service.protocol.mode = 0x12;
    assert(wheel_service_buttons(&service) == service.protocol.alternate_input.buttons);
    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    assert(wheel_service_buttons(&service) == service.protocol.packed_input.buttons);
    service.protocol.mode = 0x09;
    assert(wheel_service_buttons(&service) == service.protocol.common_input.buttons);
    service.protocol.mode = 0x0a;
    assert(wheel_service_buttons(&service) == service.protocol.common_input.buttons);
    service.protocol.mode = WHEEL_PACKET_ADAPTER_MODE;
    assert(wheel_service_buttons(&service) == service.protocol.common_input.buttons);
    service.protocol.mode = 6;
    assert(wheel_service_buttons(&service) == service.protocol.crc_input.buttons);

    service.button_banks[0] = 0xaa;
    service.button_banks[1] = 0xbb;
    service.button_banks[2] = 0xcc;
    service.protocol.common_input.buttons[0] = 1;
    service.protocol.common_input.buttons[1] = 2;
    service.protocol.common_input.buttons[2] = 3;
    service.protocol.mode = WHEEL_PACKET_METADATA_MODE;
    assert(wheel_service_buttons(&service) == service.button_banks);
    service.protocol.request_ready = false;
    service.protocol.mode = 1;
    assert(wheel_service_buttons(&service) == service.button_banks);
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

static void test_separates_scan_histories_and_updates_capability_marker(void) {
    WheelService primary;
    received_ready = false;
    uint32_t now_ms = begin_scan(&primary);
    primary.protocol.capabilities.capability_flags = 0x1234;
    primary.protocol.capabilities.report_flags = 0x5aa5;

    respond_scan(WHEEL_BUTTON_SECONDARY_RESPONSE | 0x01);
    run_service(&primary, now_ms++);
    assert(primary.primary_scan_sample_index == 0);
    assert(primary.secondary_scan_sample_index == 0);
    assert(primary.protocol.capabilities.capability_flags == 0x1206);
    assert(primary.protocol.capabilities.report_flags == 0x5aa5);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE | 0x01);
    run_service(&primary, now_ms);
    assert(primary.primary_scan_sample_index == 1);
    assert(primary.secondary_scan_sample_index == 0);

    WheelService secondary;
    now_ms = begin_scan_mode(&secondary, WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY);
    respond_scan(WHEEL_BUTTON_SECONDARY_RESPONSE | 0x01);
    run_service(&secondary, now_ms);
    assert(secondary.primary_scan_sample_index == 0);
    assert(secondary.secondary_scan_sample_index == 1);
    assert((wheel_service_capability_flags(&secondary) & 0xffu) == 6);
}

static void test_falls_back_to_scan_snapshot_and_mode_bits(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
    service.button_banks[0] = 0x12;
    service.button_banks[1] = 0x34;
    service.button_banks[2] = 0x56;

    WheelInputSnapshot snapshot;
    assert(wheel_service_input_snapshot(&service, &snapshot));
    assert(snapshot.directional_buttons == 0x12);
    assert(snapshot.secondary_buttons == 0x5634);
    assert(snapshot.clutch_paddles[0] == 0);
    assert(snapshot.tuning_input == 0);
    assert(snapshot.packed_rotary_positions == 0);
    assert(snapshot.auxiliary_report[0] == 0);
    assert(!snapshot.axis_report_enabled);
    assert(wheel_service_mode_buttons(&service) == 7);

    service.protocol.phase = WHEEL_PROTOCOL_SCANNING_SECONDARY;
    assert(wheel_service_mode_buttons(&service) == 6);

    service.protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    assert(!wheel_service_input_snapshot(&service, &snapshot));
    assert(snapshot.directional_buttons == 0);
    assert(snapshot.secondary_buttons == 0);
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

static void test_ready_clear_response_restarts_scan_state(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);
    assert(request().payload[0] == WHEEL_SCAN_PHASE_AUXILIARY);

    respond_scan(WHEEL_BUTTON_PRIMARY_RESPONSE);
    run_service(&service, now_ms++);
    assert(request().payload[0] == WHEEL_SCAN_PHASE_THIRD);

    respond_scan_not_ready();
    run_service(&service, now_ms);
    assert(request().payload[0] == WHEEL_SCAN_PHASE_AUXILIARY);
}

static void test_sends_display_output_with_each_scan_phase(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);
    const WheelDisplayOutput output = {
        .glyphs = {0xa5, 0x5a, 0x40},
        .third_glyph_marker = false,
    };
    service.protocol.common_input.report_mode = 2;
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
    uint32_t now_ms = begin_protocol_mode(&service, WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
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
    run_service(&service, now_ms++);

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

static void test_matches_official_clutch_paddle_availability(void) {
    static const uint8_t available_modes[] = {0x01, 0x02, 0x03, 0x0a, 0x13, 0x14, 0x16};
    static const uint8_t gated_modes[] = {0x00, 0x04, 0x0f, 0x10, 0x15, 0x17, 0x18, 0x1c};
    WheelService service;
    initialize_service(&service);

    for (size_t index = 0; index < sizeof(available_modes); index++) {
        service.protocol.mode = available_modes[index];
        assert(wheel_service_clutch_paddles_available(&service));
    }

    for (size_t index = 0; index < sizeof(gated_modes); index++) {
        service.protocol.mode = gated_modes[index];
        assert(!wheel_service_clutch_paddles_available(&service));
    }

    service.protocol.configured_axis_override_mode = WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED;
    assert(wheel_service_clutch_paddles_available(&service));
    service.protocol.configured_axis_override_mode = WHEEL_AXIS_OVERRIDE_MODE_NONE;
    service.protocol.adapter.connected = true;
    assert(wheel_service_clutch_paddles_available(&service));
    assert(!wheel_service_clutch_paddles_available(NULL));
}

static void test_gates_selected_modes_on_status_memory_startup(void) {
    static const uint8_t modes[] = {0x0a, 0x1c};
    for (uint8_t index = 0; index < sizeof(modes); index++) {
        WheelService service;
        uint32_t now_ms =
            begin_protocol_mode(&service, WHEEL_PROTOCOL_COMMAND_SELECT_MODE, modes[index]);

        assert(wheel_service_mode(&service) == modes[index]);
        assert(wheel_service_status_memory_startup_pending(&service));
        assert(service.protocol_deadline_active);
        WheelProtocolPhase selected_phase = wheel_service_protocol_phase(&service);
        uint32_t stale_deadline_ms = service.protocol_deadline_ms;
        respond_active(0);
        run_service(&service, now_ms++);
        assert(wheel_service_protocol_phase(&service) == selected_phase);

        bool available = index != 0;
        wheel_service_finish_status_memory_startup(&service, available);
        assert(!wheel_service_status_memory_startup_pending(&service));
        assert(!service.protocol_deadline_active);
        assert(wheel_service_tuning_menu_available(&service) == available);
        wheel_service_run(&service, stale_deadline_ms, false);
        assert(wheel_service_protocol_phase(&service) == selected_phase);
    }

    WheelService service;
    (void)begin_protocol_mode(&service, WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 0x0b);
    assert(!wheel_service_status_memory_startup_pending(&service));
    wheel_service_finish_status_memory_startup(&service, false);
    assert(service.tuning_menu_override_enabled == false);
}

static void test_publishes_packet_mode_buttons(void) {
    WheelService service;
    uint32_t now_ms = begin_protocol_mode(&service, WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);

    for (uint8_t sample = 0; sample < WHEEL_SCAN_SAMPLE_DEPTH; sample++) {
        respond_active_buttons(0x20, 0x04, 0x02);
        now_ms += 2;
        run_service(&service, now_ms);
    }

    const uint8_t *buttons = wheel_service_buttons(&service);
    assert(buttons == wheel_protocol_buttons(&service.protocol));
    assert(buttons[0] == 0x20);
    assert(buttons[1] == 0x04);
    assert(buttons[2] == 0x02);
    assert(wheel_service_acknowledgement_input_active(&service));
}

static void test_restarts_inactive_packet_mode_at_deadline(void) {
    WheelService service;
    uint32_t now_ms = begin_protocol_mode(&service, WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    run_service(&service, now_ms);

    respond_active(0);
    run_service(&service, now_ms + WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS - 1);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    run_service(&service, now_ms + WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
    assert(request().type_flags == 2);
}

static void test_ready_packet_refreshes_activity_at_deadline(void) {
    WheelService service;
    uint32_t now_ms = begin_protocol_mode(&service, WHEEL_PROTOCOL_COMMAND_SELECT_MODE, 1);
    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    now_ms++;
    run_service(&service, now_ms);

    respond_active(WHEEL_PROTOCOL_REQUEST_READY);
    now_ms += WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS + 1;
    run_service(&service, now_ms);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    run_service(&service, now_ms + WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS - 1);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_ACTIVE);

    respond_active(0);
    run_service(&service, now_ms + WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS);
    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_WAITING);
}

static void test_keeps_scan_mode_active_without_protocol_timeout(void) {
    WheelService service;
    received_ready = false;
    uint32_t now_ms = begin_scan(&service);
    assert(service.protocol_deadline_active);
    run_service(&service, now_ms + 2001);

    assert(wheel_service_protocol_phase(&service) == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(request().type_flags == 3);
}

static void test_requires_active_phase_for_startup_discovery_completion(void) {
    static const WheelProtocolPhase waiting_phases[] = {
        WHEEL_PROTOCOL_WAITING,          WHEEL_PROTOCOL_SYNCHRONIZING,
        WHEEL_PROTOCOL_ACKNOWLEDGING,    WHEEL_PROTOCOL_SELECTING,
        WHEEL_PROTOCOL_AUTHENTICATING,   WHEEL_PROTOCOL_UNSUPPORTED,
        WHEEL_PROTOCOL_SCANNING_PRIMARY, WHEEL_PROTOCOL_SCANNING_SECONDARY,
    };
    WheelService service;
    initialize_service(&service);

    service.protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    assert(wheel_service_startup_discovery_complete(&service));
    service.status_memory_startup_pending = true;
    assert(wheel_service_startup_discovery_complete(&service));

    for (uint8_t index = 0; index < sizeof(waiting_phases) / sizeof(waiting_phases[0]); index++) {
        service.protocol.phase = waiting_phases[index];
        assert(!wheel_service_startup_discovery_complete(&service));
    }
    assert(!wheel_service_startup_discovery_complete(NULL));
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
    assert(!wheel_service_multi_position_supported(&service));
    assert(wheel_service_multi_position_mode(&service, TUNING_MULTI_POSITION_CONSTANT) ==
           TUNING_MULTI_POSITION_ENCODER);
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
    service.protocol.request[13] = 0xac;
    service.protocol.request[14] = 0x01;
    WheelMultiPositionInput input;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.channels[0].position == 3);
    assert(input.channels[1].position == 10);
    assert(input.channels[2].position == 12);
    assert(input.channels[0].event == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[1].event == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[2].event == WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_service_rotary_event(&service, 3) == WHEEL_ROTARY_EVENT_NONE);
    assert(input.channels[0].active);
    assert(input.channels[1].active);
    assert(input.channels[2].active);

    service.protocol.request[6] = 4;
    service.protocol.request[7] = 9;
    service.protocol.request[13] = 0x21;
    service.protocol.request[14] = 0;
    assert(wheel_service_multi_position_input(&service, 1, &input));
    assert(input.channels[0].event == WHEEL_ROTARY_EVENT_FORWARD);
    assert(input.channels[1].event == WHEEL_ROTARY_EVENT_BACKWARD);
    assert(input.channels[2].event == WHEEL_ROTARY_EVENT_FORWARD);
    assert(wheel_service_rotary_event(&service, 3) == WHEEL_ROTARY_EVENT_NONE);
    assert(service.rotary_input.channels[3].position == UINT8_MAX);
}

static void test_gates_quaternary_rotary_to_legacy_mode(void) {
    WheelService service;
    WheelMultiPositionInput input;
    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    service.protocol.request[13] = 0x10;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(service.rotary_input.channels[3].position == UINT8_MAX);
    assert(service.rotary_input.channels[3].pending_steps == 0);
    assert(service.rotary_input.channels[3].event == WHEEL_ROTARY_EVENT_NONE);
    assert(service.rotary_input.channels[3].phase == WHEEL_ROTARY_PHASE_IDLE);
    assert(service.rotary_input.channels[3].deadline_ms == 0);

    service.protocol.request[13] = 0x20;
    assert(wheel_service_multi_position_input(&service, 1, &input));
    assert(service.rotary_input.channels[3].position == UINT8_MAX);
    assert(service.rotary_input.channels[3].pending_steps == 0);
    assert(service.rotary_input.channels[3].event == WHEEL_ROTARY_EVENT_NONE);
    assert(service.rotary_input.channels[3].phase == WHEEL_ROTARY_PHASE_IDLE);
    assert(service.rotary_input.channels[3].deadline_ms == 0);

    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = WHEEL_MODE_REMOTE_TUNING_LEGACY;
    service.protocol.request[13] = 0x10;
    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(service.rotary_input.channels[3].position == 1);
    assert(wheel_service_rotary_event(&service, 3) == WHEEL_ROTARY_EVENT_NONE);

    service.protocol.request[13] = 0x20;
    assert(wheel_service_multi_position_input(&service, 1, &input));
    assert(service.rotary_input.channels[3].position == 2);
    assert(wheel_service_rotary_event(&service, 3) == WHEEL_ROTARY_EVENT_FORWARD);
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
    service.protocol.adapter.rotary_positions[0] = 0xa5;
    service.protocol.adapter.rotary_positions[1] = 0xb7;
    WheelMultiPositionInput input;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.channels[0].position == 5);
    assert(input.channels[1].position == 10);
    assert(input.channels[2].position == 7);
    assert(input.channels[2].active);
}

static void test_builds_adapter_normal_multi_position_input(void) {
    static const uint8_t normal_modes[] = {9, 10, 11, 12, 0x0e, 0x0f, 0x17, 0x1b, 0x1c, 0x1d};
    WheelService service;
    WheelMultiPositionInput input;

    for (size_t index = 0; index < sizeof(normal_modes) / sizeof(normal_modes[0]); index++) {
        initialize_service(&service);
        service.protocol.request_ready = true;
        service.protocol.mode = normal_modes[index];
        service.protocol.request[6] = 1;
        service.protocol.request[7] = 2;
        service.protocol.request[13] = 0xac;
        service.protocol.request[14] = 3;
        service.protocol.adapter.connected = true;
        service.protocol.adapter.mode = 0;
        service.protocol.adapter.rotary_positions[0] = 0xa5;
        service.protocol.adapter.rotary_positions[1] = 0xb7;

        assert(wheel_service_multi_position_input(&service, 0, &input));
        assert(input.channels[0].position == 5);
        assert(input.channels[1].position == 10);
        assert(input.channels[2].position == 12);
        assert(input.channels[2].active ==
               (normal_modes[index] == 0x0f || normal_modes[index] == 0x17 ||
                normal_modes[index] == 0x1c));
    }

    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = 0x1c;
    service.protocol.request[6] = 1;
    service.protocol.request[7] = 2;
    service.protocol.request[13] = 0xac;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    service.protocol.adapter.rotary_positions[0] = 0xa5;
    service.protocol.adapter.rotary_positions[1] = 0xb7;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.channels[0].position == 5);
    assert(input.channels[1].position == 10);
    assert(input.channels[2].position == 7);
    assert(input.channels[2].active);

    initialize_service(&service);
    service.protocol.request_ready = true;
    service.protocol.mode = 0x10;
    service.protocol.request[6] = 1;
    service.protocol.request[7] = 2;
    service.protocol.request[13] = 0xac;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    service.protocol.adapter.rotary_positions[0] = 0xa5;
    service.protocol.adapter.rotary_positions[1] = 0xb7;

    assert(wheel_service_multi_position_input(&service, 0, &input));
    assert(input.channels[0].position == 1);
    assert(input.channels[1].position == 2);
    assert(input.channels[2].position == 12);
    assert(!input.channels[2].active);
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

static void test_discards_host_motion_without_resetting_rotary_state(void) {
    WheelService service;
    initialize_service(&service);
    service.protocol.motion = (WheelMotion){.primary = 2, .axes = {3, 4, 5, 6}};
    for (uint8_t channel = 0; channel < WHEEL_ROTARY_INPUT_CHANNEL_COUNT; channel++) {
        service.rotary_input.channels[channel] = (WheelRotaryChannel){
            .deadline_ms = (uint32_t)(100 + channel),
            .pending_steps = (int8_t)(channel + 1),
            .position = (uint8_t)(channel + 2),
            .event = (WheelRotaryEvent)(channel % 3),
            .phase = (WheelRotaryPhase)(channel % 3),
        };
    }
    WheelRotaryInput expected_rotary_input = service.rotary_input;

    wheel_service_discard_host_motion(&service);

    assert(service.protocol.motion.primary == 0);
    for (uint8_t axis = 0; axis < WHEEL_MOTION_AXIS_COUNT; axis++) {
        assert(service.protocol.motion.axes[axis] == 0);
    }
    for (uint8_t channel = 0; channel < WHEEL_ROTARY_INPUT_CHANNEL_COUNT; channel++) {
        assert(service.rotary_input.channels[channel].deadline_ms ==
               expected_rotary_input.channels[channel].deadline_ms);
        assert(service.rotary_input.channels[channel].pending_steps ==
               expected_rotary_input.channels[channel].pending_steps);
        assert(service.rotary_input.channels[channel].position ==
               expected_rotary_input.channels[channel].position);
        assert(service.rotary_input.channels[channel].event ==
               expected_rotary_input.channels[channel].event);
        assert(service.rotary_input.channels[channel].phase ==
               expected_rotary_input.channels[channel].phase);
    }
    wheel_service_discard_host_motion(NULL);
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

    wheel_service_set_display_report(&service, 0x2468);
    service.protocol.crc_output.status_update_pending = false;
    assert(wheel_service_apply_packed_report_command(&service, &command));
    assert(service.protocol.output_reports.report_two[0] == 0xff);
    assert(service.protocol.output_reports.report_two[1] == 0xff);
    assert(service.adapter_commands.report_two_pending);
    assert(service.protocol.adapter_output.display_report == 0);
    assert(!service.protocol.crc_output.status_update_pending);
    assert(memcmp(service.adapter_commands.report_two, service.protocol.output_reports.report_two,
                  WHEEL_OUTPUT_REPORT_TWO_SIZE) == 0);

    initialize_service(&service);
    service.protocol.mode = WHEEL_MODE_LEGACY_ALTERNATE;
    wheel_service_set_display_report(&service, 0x1357);
    service.protocol.crc_output.status_update_pending = false;
    assert(wheel_service_apply_packed_report_command(&service, &command));
    assert(!service.adapter_commands.report_two_pending);
    assert(service.protocol.adapter_output.display_report == 0x1357);
    assert(!service.protocol.crc_output.status_update_pending);
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
    wheel_service_set_auxiliary_report(&service, 0x5634);
    service.protocol.crc_output.status_update_pending = false;
    UsbOperatingModeCommand command = {
        .opcode = WHEEL_AUXILIARY_OPTION_OPCODE,
        .parameters = {2, 0, 0, 0},
    };

    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 1);
    assert(service.protocol.alternate_output.suppress_auxiliary_display);

    command.parameters[0] = 1;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 1);
    assert(service.protocol.alternate_output.suppress_auxiliary_display);

    command.parameters[0] = 0;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 0);
    assert(!service.protocol.alternate_output.suppress_auxiliary_display);

    command.parameters[0] = UINT8_MAX;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.option == 1);
    assert(service.protocol.alternate_output.suppress_auxiliary_display);

    command.opcode = WHEEL_AUXILIARY_CODE_MODE_OPCODE;
    command.parameters[0] = UINT8_MAX;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.code_mode);

    command.opcode = WHEEL_AUXILIARY_REPORT_OPCODE;
    command.parameters[0] = 0x01;
    command.parameters[1] = 0x34;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.auxiliary_output.report == 0x5634);
    assert(service.protocol.mode_one_output.vibration[0] == 0x34);
    assert(service.protocol.mode_one_output.vibration[1] == 0x56);
    assert(service.protocol.alternate_output.display.auxiliary == 0x34);
    assert(!service.protocol.alternate_output.auxiliary_status);
    assert(service.protocol.adapter_output.display_report == 0x0134);
    assert(service.protocol.crc_output.status_update_pending);
    service.protocol.crc_output.status_update_pending = false;
    assert(wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(service.protocol.crc_output.status_update_pending);

    command.opcode = 0x09;
    assert(!wheel_service_apply_auxiliary_output_command(&service, &command));
    assert(!wheel_service_apply_auxiliary_output_command(NULL, &command));
    assert(!wheel_service_apply_auxiliary_output_command(&service, NULL));
}

static void test_restores_retained_output_settings_after_service_reset(void) {
    WheelService service;
    initialize_service(&service);

    wheel_service_set_auxiliary_output_option(&service, 2);
    wheel_service_set_button_illumination(&service, true);
    assert(service.auxiliary_output.option == 1);
    assert(service.protocol.alternate_output.suppress_auxiliary_display);
    assert(service.protocol.output_reports.button_illumination);

    wheel_service_init(&service, &transport);
    assert(service.auxiliary_output.option == 0);
    assert(!service.protocol.alternate_output.suppress_auxiliary_display);
    assert(!service.protocol.output_reports.button_illumination);

    wheel_service_set_auxiliary_output_option(&service, 2);
    wheel_service_set_button_illumination(&service, true);
    assert(service.auxiliary_output.option == 1);
    assert(service.protocol.alternate_output.suppress_auxiliary_display);
    assert(service.protocol.output_reports.button_illumination);
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
    assert(wheel_service_rotary_event(&service, WHEEL_ROTARY_INPUT_CHANNEL_COUNT) ==
           WHEEL_ROTARY_EVENT_NONE);
    assert(wheel_service_rotary_event(NULL, 0) == WHEEL_ROTARY_EVENT_NONE);
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

static void test_exposes_extended_pulse_flags(void) {
    WheelService service;
    initialize_service(&service);

    assert(wheel_service_extended_pulse_flags(NULL) == 0);
    assert(wheel_service_extended_pulse_flags(&service) == 0);

    service.protocol.extended_pulse_state.active_flags = 0xc3;
    assert(wheel_service_extended_pulse_flags(&service) == 0xc3);
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

static void test_activates_xbox_gip_display_report(void) {
    WheelService service = {0};
    service.protocol.mode = 9;

    wheel_service_activate_xbox_gip_display(&service);
    assert(service.protocol.adapter_output.display_report == 0x2040);
    assert(!service.protocol.crc_output.status_update_pending);

    service.protocol.adapter_output.display_report = 0x001e;
    wheel_service_activate_xbox_gip_display(&service);
    assert(service.protocol.adapter_output.display_report == 0x2042);

    service.protocol.adapter_output.display_report = 0x8020;
    wheel_service_activate_xbox_gip_display(&service);
    assert(service.protocol.adapter_output.display_report == 0xa060);

    const uint16_t inhibited_reports[] = {0x0001, 0x1000, 0x1001};
    for (size_t index = 0; index < sizeof(inhibited_reports) / sizeof(inhibited_reports[0]);
         index++) {
        service.protocol.adapter_output.display_report = inhibited_reports[index];
        wheel_service_activate_xbox_gip_display(&service);
        assert(service.protocol.adapter_output.display_report == inhibited_reports[index]);
    }

    service.protocol.mode = 24;
    service.protocol.adapter_output.display_report = 0;
    wheel_service_activate_xbox_gip_display(&service);
    assert(service.protocol.adapter_output.display_report == 0);

    service.protocol.mode = 4;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    wheel_service_activate_xbox_gip_display(&service);
    assert(service.protocol.adapter_output.display_report == 0x2040);
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

    assert(wheel_service_queue_tuning_display_command(&service, 0x07));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[1] == 0x82);
    assert(frame[2] == 0x07);

    assert(wheel_service_queue_tuning_display_command(&service, 0x11));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[1] == 0x82);
    assert(frame[2] == 0x11);

    service.protocol.mode = 4;
    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    assert(!wheel_service_queue_tuning_display_command(&service, 0x0a));
    assert(wheel_service_queue_adapter_text_line(&service, 1, 0x10, text, sizeof(text)));
    assert(service.adapter_commands.text_lines_pending == 1);
    assert(wheel_service_queue_adapter_text_close(&service));
    assert(service.adapter_commands.text_close_pending);
}

static void test_queues_selected_tuning_configuration_commands(void) {
    WheelService service = {0};
    TuningProfileBank bank = {0};
    uint8_t frame[33] = {0};

    service.protocol.mode = 16;
    bank.selected_slot = 0;
    bank.active_slot = 0;
    assert(wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_SENSITIVITY,
                                                             &bank, false));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[0] == 0xa6);
    assert(frame[1] == 0x82);
    assert(frame[2] == TUNING_ENTRY_SENSITIVITY);

    assert(wheel_service_queue_selected_tuning_configuration(
        &service, TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, &bank, false));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[2] == TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH);

    bank.selected_slot = 1;
    bank.active_slot = 1;
    assert(wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_SETUP, &bank,
                                                             false));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[2] == 26);

    bank.selected_slot = 2;
    bank.active_slot = 2;
    assert(wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_SETUP, &bank,
                                                             false));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[2] == 27);

    bank.selected_slot = 0;
    bank.active_slot = 0;
    assert(wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_SETUP, &bank,
                                                             true));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[2] == 0);

    bank.selected_slot = 1;
    bank.active_slot = 1;
    assert(wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_SETUP, &bank,
                                                             true));
    assert(wheel_output_reports_encode_next(&service.protocol.output_reports, 16, frame));
    assert(frame[2] == TUNING_ENTRY_COUNT);

    assert(!wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_SETUP, NULL,
                                                              false));
    assert(!wheel_service_queue_selected_tuning_configuration(&service, TUNING_ENTRY_COUNT, &bank,
                                                              false));
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

    service.protocol.adapter.connected = true;
    service.protocol.adapter.mode = 1;
    service.protocol.adapter.buttons[1] = 0x01;
    assert(wheel_service_calibration_advance_input_active(&service));
    service.protocol.adapter.buttons[1] = 0;
    service.button_banks[1] = 0x80;
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

static void test_keeps_vibration_independent_from_display_report(void) {
    WheelService service;
    initialize_service(&service);
    const WheelVibrationOutput output = {.channels = {0x34, 0x56}};

    wheel_service_set_vibration_output(&service, &output);

    assert(service.protocol.mode_one_output.vibration[0] == 0x34);
    assert(service.protocol.mode_one_output.vibration[1] == 0x56);
    assert(service.auxiliary_output.report == 0x5634);
    assert(service.protocol.alternate_output.display.auxiliary == 0x34);
    assert(!service.protocol.alternate_output.auxiliary_status);
    assert(service.protocol.adapter_output.display_report == 0);

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

static void test_synchronizes_exclusive_auxiliary_mode(void) {
    WheelService service;
    initialize_service(&service);

    service.auxiliary_output.latched_bands = 0x07;
    wheel_service_set_auxiliary_exclusive_mode(&service, true);
    assert(service.auxiliary_output.exclusive_mode);
    assert(service.auxiliary_output.latched_bands == 0x07);

    wheel_service_set_auxiliary_exclusive_mode(&service, false);
    assert(!service.auxiliary_output.exclusive_mode);
    assert(service.auxiliary_output.latched_bands == 0);
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
    wheel_service_set_display_report(&service, 0x2468);
    service.protocol.crc_output.status_update_pending = false;
    wheel_service_set_legacy_axes(&service, axes);
    wheel_service_set_display_output(&service, &display);
    assert(wheel_output_reports_queue_packed(&service.protocol.output_reports, 2, packed,
                                             service.protocol.mode));
    assert(wheel_output_reports_queue_packed(&service.protocol.output_reports, 1, packed,
                                             service.protocol.mode));

    wheel_service_reset_host_protocol_outputs(&service);

    assert(service.auxiliary_output.report == 0);
    assert(service.protocol.adapter_output.display_report == 0);
    assert(service.protocol.crc_output.status_update_pending);
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

    service.protocol.adapter.buttons[0] = 0x80;
    assert(!wheel_service_adapter_requests_host_capability(&service));
    service.protocol.adapter.connected = true;
    assert(wheel_service_adapter_requests_host_capability(&service));
    service.protocol.adapter.buttons[0] = 0x7f;
    assert(!wheel_service_adapter_requests_host_capability(&service));
}

static void test_reports_force_output_readiness(void) {
    WheelService service;
    initialize_service(&service);

    service.protocol.phase = WHEEL_PROTOCOL_WAITING;
    assert(!wheel_service_force_output_ready(&service));
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_SYNCHRONIZING;
    assert(!wheel_service_force_output_ready(&service));
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_ACKNOWLEDGING;
    assert(!wheel_service_force_output_ready(&service));
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_SELECTING;
    assert(!wheel_service_force_output_ready(&service));
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_UNSUPPORTED;
    assert(!wheel_service_force_output_ready(&service));
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_AUTHENTICATING;
    assert(wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    assert(wheel_service_force_output_ready(&service));
    assert(!wheel_service_force_output_transition_active(&service));
    service.protocol.command_invalid = true;
    assert(wheel_service_force_output_transition_active(&service));
    service.protocol.command_invalid = false;
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
    service.protocol.request[13] = 0xa5;
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
    assert(snapshot.packed_rotary_positions == 0xa5);
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
    assert(!wheel_service_input_snapshot(NULL, &snapshot));
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
    test_exposes_protocol_bridge_report_id();
    test_applies_scan_status_flags_once();
    test_omits_scan_status_flags_when_available();
    test_retries_scan_after_transport_start_failure();
    test_retries_protocol_after_transport_start_failure();
    test_paces_logical_protocol_requests();
    test_recovers_malformed_protocol_response_and_clears_completion();
    test_resets_transient_display_state();
    test_recovers_unknown_selection_after_deadline();
    test_maps_primary_scan_bits();
    test_maps_secondary_scan_bit();
    test_separates_scan_histories_and_updates_capability_marker();
    test_falls_back_to_scan_snapshot_and_mode_bits();
    test_negotiates_before_scanning_and_maps_buttons();
    test_releases_scan_button_on_first_zero();
    test_ready_clear_response_restarts_scan_state();
    test_sends_display_output_with_each_scan_phase();
    test_keeps_protocol_transport_for_packet_modes();
    test_matches_official_clutch_paddle_availability();
    test_gates_selected_modes_on_status_memory_startup();
    test_publishes_packet_mode_buttons();
    test_delegates_protocol_button_families_and_scan_fallback();
    test_restarts_inactive_packet_mode_at_deadline();
    test_ready_packet_refreshes_activity_at_deadline();
    test_keeps_scan_mode_active_without_protocol_timeout();
    test_requires_active_phase_for_startup_discovery_completion();
    test_defers_next_request_for_shared_serial_work();
    test_forces_and_reports_protocol_exchange();
    test_initializes_rotary_input();
    test_routes_multi_position_mode();
    test_builds_direct_multi_position_input();
    test_gates_quaternary_rotary_to_legacy_mode();
    test_builds_adapter_multi_position_input();
    test_builds_adapter_normal_multi_position_input();
    test_marks_extended_multi_position_layout();
    test_discards_host_motion_without_resetting_rotary_state();
    test_filters_adapter_remote_tuning_active_state();
    test_retains_adapter_display_state_across_command_resets();
    test_mirrors_standard_adapter_output_reports();
    test_mirrors_extended_adapter_output_reports();
    test_routes_auxiliary_output_commands();
    test_restores_retained_output_settings_after_service_reset();
    test_routes_packed_report_commands();
    test_routes_report_six_command();
    test_routes_and_toggles_interface_mode_gate();
    test_rejects_unavailable_multi_position_input();
    test_selects_extended_report_fields();
    test_exposes_extended_pulse_flags();
    test_reports_calibration_availability();
    test_gates_torque_key_acknowledgement();
    test_reports_tuning_display_support();
    test_activates_xbox_gip_display_report();
    test_routes_tuning_display_output_by_connection();
    test_queues_selected_tuning_configuration_commands();
    test_activates_interface_presentation_by_connection();
    test_selects_calibration_advance_button_by_wheel_mode();
    test_reports_mode_gated_input_capability();
    test_exposes_axis_overrides();
    test_reports_bite_point_adjustment();
    test_keeps_vibration_independent_from_display_report();
    test_applies_legacy_axes_to_every_packet_family();
    test_synchronizes_exclusive_auxiliary_mode();
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
