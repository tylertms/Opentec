#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_mode_one.h"
#include "wheel/protocol.h"

static void mark_ready(uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    packet[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
}

static void synchronize(WheelProtocol *protocol, uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    mark_ready(request);
    wheel_protocol_accept(protocol, request);
    assert(protocol->phase == WHEEL_PROTOCOL_SYNCHRONIZING);
    assert(wheel_protocol_response(protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] == 0);

    wheel_protocol_accept(protocol, request);
    assert(protocol->phase == WHEEL_PROTOCOL_ACKNOWLEDGING);
    assert(wheel_protocol_response(protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] ==
           WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED);

    wheel_protocol_accept(protocol, request);
    assert(protocol->phase == WHEEL_PROTOCOL_SELECTING);
}

static void select_mode(WheelProtocol *protocol, uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE],
                        uint8_t mode) {
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[1] = mode;
    wheel_protocol_accept(protocol, request);
}

static void begin_authentication(WheelProtocol *protocol,
                                 uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE], uint8_t wheel_mode) {
    static const uint8_t nonce[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    wheel_protocol_init(protocol);
    synchronize(protocol, request);
    select_mode(protocol, request, wheel_mode);

    memset(request, 0, WHEEL_PROTOCOL_PACKET_SIZE);
    request[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    memcpy(&request[2], nonce, sizeof(nonce));
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(protocol, request);
}

static void complete_mode_10_authentication(WheelProtocol *protocol,
                                            uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    static const uint8_t encrypted_proof[WHEEL_PROTOCOL_CONTENT_SIZE] = {
        0xbf, 0xd4, 0x29, 0xc4, 0x2b, 0xd6, 0x73, 0xf4, 0x4c, 0x15, 0x73,
        0xc4, 0x4b, 0x01, 0xd6, 0x85, 0xcb, 0xba, 0x7a, 0x8b, 0x92, 0xf2,
        0xb5, 0x14, 0xfb, 0xaf, 0x35, 0x42, 0xfa, 0xf8, 0x7b, 0x36,
    };
    begin_authentication(protocol, request, 0x10);
    memset(request, 0, WHEEL_PROTOCOL_PACKET_SIZE);
    memcpy(request, encrypted_proof, sizeof(encrypted_proof));
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(protocol, request);
}

static void test_synchronizes_and_selects_mode(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);

    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(protocol.mode == 1);
    assert(wheel_protocol_response(&protocol)[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0x9a);
    assert(wheel_protocol_message_valid(wheel_protocol_response(&protocol)));
}

static void test_selects_scan_variants(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);

    request[0] = WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY);
    assert(protocol.mode == WHEEL_MODE_SCAN_PRIMARY);

    wheel_protocol_init(&protocol);
    memset(request, 0, sizeof(request));
    synchronize(&protocol, request);
    request[0] = WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY;
    wheel_protocol_accept(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY);
    assert(protocol.mode == WHEEL_MODE_SCAN_SECONDARY);
}

static void test_selects_authentication_from_wheel_mode(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 0x10);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(protocol.mode == 0x10);

    wheel_protocol_init(&protocol);
    memset(request, 0, sizeof(request));
    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(protocol.mode == 1);
}

static void test_authentication_mode_set(void) {
    for (uint16_t mode = 0; mode <= UINT8_MAX; mode++) {
        bool expected = (mode >= 0x0a && mode <= 0x0c) || (mode >= 0x0e && mode <= 0x17) ||
                        (mode >= 0x1b && mode <= 0x1e);
        assert(wheel_authentication_required((uint8_t)mode) == expected);
    }
}

static void test_selects_authentication_keys_for_each_wheel_mode(void) {
    static const struct {
        uint8_t wheel_mode;
        uint8_t response_prefix[4];
    } cases[] = {
        {0x0a, {0xb5, 0x8e, 0x45, 0x9b}}, {0x0b, {0xb5, 0x8e, 0x45, 0x9b}},
        {0x0c, {0x24, 0x9c, 0x63, 0xd4}}, {0x0e, {0x4e, 0x2b, 0xce, 0x4e}},
        {0x0f, {0xef, 0x40, 0xb4, 0x3e}}, {0x10, {0xdf, 0x6b, 0xb6, 0x58}},
        {0x11, {0xc9, 0x8e, 0xd6, 0xa3}}, {0x12, {0x19, 0x8d, 0x07, 0xea}},
        {0x13, {0xc8, 0x2d, 0xce, 0x0d}}, {0x14, {0x8e, 0xa8, 0xd7, 0xd3}},
        {0x15, {0x1d, 0xdb, 0xe6, 0x33}}, {0x16, {0x46, 0x50, 0xc4, 0x43}},
        {0x17, {0xef, 0x40, 0xb4, 0x3e}}, {0x1b, {0xb5, 0x8e, 0x45, 0x9b}},
        {0x1c, {0xb5, 0x8e, 0x45, 0x9b}}, {0x1d, {0xb5, 0x8e, 0x45, 0x9b}},
        {0x1e, {0xdf, 0x6b, 0xb6, 0x58}},
    };

    for (uint8_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        WheelProtocol protocol;
        uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
        begin_authentication(&protocol, request, cases[index].wheel_mode);
        assert(memcmp(wheel_protocol_response(&protocol), cases[index].response_prefix,
                      sizeof(cases[index].response_prefix)) == 0);
    }
}

static void test_completes_encrypted_authentication_exchange(void) {
    static const uint8_t expected_challenge_response[WHEEL_PROTOCOL_CONTENT_SIZE] = {
        0xdf, 0x6b, 0xb6, 0x58, 0xf4, 0xf6, 0x0e, 0xe4, 0x4b, 0x22, 0x58,
        0x94, 0xea, 0xe6, 0x84, 0xde, 0xcb, 0x45, 0x88, 0xc4, 0x4a, 0x4c,
        0x42, 0x94, 0xc1, 0xfa, 0x17, 0xb3, 0x57, 0x2c, 0x52, 0xa0,
    };
    static const uint8_t encrypted_proof[WHEEL_PROTOCOL_CONTENT_SIZE] = {
        0xbf, 0xd4, 0x29, 0xc4, 0x2b, 0xd6, 0x73, 0xf4, 0x4c, 0x15, 0x73,
        0xc4, 0x4b, 0x01, 0xd6, 0x85, 0xcb, 0xba, 0x7a, 0x8b, 0x92, 0xf2,
        0xb5, 0x14, 0xfb, 0xaf, 0x35, 0x42, 0xfa, 0xf8, 0x7b, 0x36,
    };
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    begin_authentication(&protocol, request, 0x10);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(memcmp(wheel_protocol_response(&protocol), expected_challenge_response,
                  sizeof(expected_challenge_response)) == 0);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0xd0);

    memset(request, 0, sizeof(request));
    memcpy(request, encrypted_proof, sizeof(encrypted_proof));
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_protocol_response(&protocol)[0] == 0xdf);
    assert(wheel_protocol_response(&protocol)[1] == 0x6b);
    for (uint8_t index = 2; index < WHEEL_PROTOCOL_CONTENT_SIZE; index++) {
        assert(wheel_protocol_response(&protocol)[index] == 0);
    }
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0xbb);
    assert(wheel_protocol_message_valid(wheel_protocol_response(&protocol)));
}

static void test_retries_invalid_authentication_challenge(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 0x10);

    memset(request, 0, sizeof(request));
    request[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        (uint8_t)(wheel_protocol_message_checksum(request) ^ UINT8_MAX);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(protocol.authentication.stage == WHEEL_AUTHENTICATION_AWAITING_CHALLENGE);
    assert(protocol.authentication.retry_counter[7] == 1);
    assert(wheel_protocol_response(&protocol)[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE);
    assert(wheel_protocol_response(&protocol)[1] == 0);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0x9a);
}

static void test_retries_invalid_encrypted_proof(void) {
    static const uint8_t expected_response[WHEEL_PROTOCOL_CONTENT_SIZE] = {
        0xf1, 0x0e, 0x42, 0x81, 0xa0, 0xb8, 0xc5, 0x81, 0x4f, 0x34, 0xa0,
        0x94, 0x07, 0x3f, 0x22, 0x42, 0x29, 0x6b, 0x55, 0x14, 0xf8, 0xc9,
        0xe6, 0x16, 0xfd, 0x02, 0x82, 0x7d, 0x67, 0xb4, 0x1f, 0x20,
    };
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    begin_authentication(&protocol, request, 0x10);

    memset(request, 0, sizeof(request));
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        (uint8_t)(wheel_protocol_message_checksum(request) ^ UINT8_MAX);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(protocol.authentication.stage == WHEEL_AUTHENTICATION_AWAITING_PROOF);
    assert(protocol.authentication.retry_counter[7] == 1);
    assert(memcmp(wheel_protocol_response(&protocol), expected_response,
                  sizeof(expected_response)) == 0);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0x4b);
}

static void test_echoes_token_from_wrong_encrypted_command(void) {
    static const uint8_t encrypted_request[WHEEL_PROTOCOL_CONTENT_SIZE] = {
        0xbe, 0xd4, 0x29, 0xc4, 0x2b, 0xd6, 0x73, 0xf4, 0x4c, 0x15, 0x73,
        0xc4, 0x4b, 0x01, 0xd6, 0x85, 0xcb, 0xba, 0x7a, 0x8b, 0x92, 0xf2,
        0xb5, 0x14, 0xfb, 0xaf, 0x35, 0x42, 0xfa, 0xf8, 0x7b, 0x36,
    };
    static const uint8_t expected_response[WHEEL_PROTOCOL_CONTENT_SIZE] = {
        0x8b, 0x65, 0xe5, 0xfb, 0x67, 0x0a, 0x9e, 0x03, 0x73, 0x9e, 0xa0,
        0x94, 0x07, 0x3f, 0x22, 0x42, 0x29, 0x6b, 0x55, 0x14, 0xf8, 0xc9,
        0xe6, 0x16, 0xfd, 0x02, 0x82, 0x7d, 0x67, 0xb4, 0x1f, 0x20,
    };
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    begin_authentication(&protocol, request, 0x10);

    memset(request, 0, sizeof(request));
    memcpy(request, encrypted_request, sizeof(encrypted_request));
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(memcmp(wheel_protocol_response(&protocol), expected_response,
                  sizeof(expected_response)) == 0);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == 0x8b);
}

static void test_accepts_authenticated_active_commands_and_restarts_authentication(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    complete_mode_10_authentication(&protocol, request);
    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);

    memset(request, 0, sizeof(request));
    request[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    request[2] = 0x41;
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_protocol_request(&protocol) != 0);
    assert(wheel_protocol_request(&protocol)[0] == 0);
    assert(wheel_protocol_request(&protocol)[2] == 0);

    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING);
    assert(protocol.authentication.stage == WHEEL_AUTHENTICATION_AWAITING_CHALLENGE);
    assert(wheel_protocol_message_valid(wheel_protocol_response(&protocol)));
}

static void test_refreshes_active_response_after_invalid_checksum(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    const WheelPacketModeOneOutput output = {
        .display = {.glyphs = {0x12, 0x34, 0x56}},
    };
    wheel_protocol_set_mode_one_output(&protocol, &output);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        (uint8_t)(wheel_protocol_message_checksum(request) ^ UINT8_MAX);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_protocol_request(&protocol) == 0);
    assert(wheel_protocol_response(&protocol)[2] == 0x12);
    assert(wheel_protocol_response(&protocol)[3] == 0x34);
    assert(wheel_protocol_response(&protocol)[4] == 0x56);
    assert(wheel_protocol_message_valid(wheel_protocol_response(&protocol)));
}

static void test_restarts_synchronization_when_ready_drops(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    mark_ready(request);
    wheel_protocol_accept(&protocol, request);
    wheel_protocol_accept(&protocol, (uint8_t[WHEEL_PROTOCOL_PACKET_SIZE]){0});

    assert(protocol.phase == WHEEL_PROTOCOL_WAITING);
    assert(protocol.mode == WHEEL_MODE_UNKNOWN);
    assert(wheel_protocol_response(&protocol)[WHEEL_PROTOCOL_FLAGS_OFFSET] == 0);
}

static void test_captures_normalized_active_requests(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    for (uint8_t index = 0; index < WHEEL_PROTOCOL_CONTENT_SIZE; index++) {
        request[index] = index;
    }
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    mark_ready(request);
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    assert(protocol.phase == WHEEL_PROTOCOL_ACTIVE);
    assert(wheel_protocol_request(&protocol) != 0);
    const uint8_t expected[WHEEL_PROTOCOL_SNAPSHOT_SIZE] = {
        0, 0, 0, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31,
    };
    assert(memcmp(wheel_protocol_request(&protocol), expected, sizeof(expected)) == 0);
    const WheelPacketModeOneInput *input = wheel_protocol_mode_one_input(&protocol);
    assert(input != 0);
    assert(input->buttons[0] == 0);
    assert(input->buttons[2] == 0);
    assert(input->motion == 7);
    assert(input->axis_values[0] == 0);
    assert(input->axis_limit == 31);
    const WheelPacketModeOneReportState *report_state =
        wheel_protocol_mode_one_report_state(&protocol);
    assert(report_state != 0);
    assert(report_state->axis_values[0] == 0x1312);
    assert(report_state->axis_values[1] == 0x1514);
    assert(report_state->report_mode == 28);
    assert(report_state->report_capabilities == 30);
    assert(report_state->axis_limit == 31);
    assert(wheel_protocol_request_changed(&protocol));
    assert(!wheel_protocol_request_changed(&protocol));

    wheel_protocol_accept(&protocol, request);
    assert(!wheel_protocol_request_changed(&protocol));
    assert(wheel_protocol_mode_one_input(&protocol)->buttons[0] == 0);
    wheel_protocol_accept(&protocol, request);
    assert(wheel_protocol_request_changed(&protocol));
    assert(wheel_protocol_mode_one_input(&protocol)->buttons[0] == 2);
    assert(wheel_protocol_mode_one_input(&protocol)->buttons[2] == 4);
    request[29]++;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);
    assert(!wheel_protocol_request_changed(&protocol));
    request[31]++;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);
    assert(wheel_protocol_request_changed(&protocol));
}

static void test_averages_control_axes_only_for_authenticated_wheel_modes(void) {
    WheelProtocol protocol;
    wheel_protocol_init(&protocol);
    protocol.mode = 0x13;
    protocol.phase = WHEEL_PROTOCOL_ACTIVE;

    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    request[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    request[12] = 90;
    request[13] = 30;
    request[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);
    const WheelPacketModeOneInput *input = wheel_protocol_mode_one_input(&protocol);
    assert(input->controls.x == 0);
    assert(input->controls.y == 0);

    wheel_protocol_init(&protocol);
    protocol.mode = 1;
    protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);
    input = wheel_protocol_mode_one_input(&protocol);
    assert(input->controls.x == 0);
    assert(input->controls.y == 0);
}

static void accept_active_request(WheelProtocol *protocol,
                                  uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(protocol, request);
}

static void test_tracks_display_acknowledgement_input(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    protocol.mode = 1;
    protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;

    accept_active_request(&protocol, request);
    assert(!wheel_protocol_acknowledgement_input_active(&protocol));

    request[11] = 1;
    accept_active_request(&protocol, request);
    assert(wheel_protocol_acknowledgement_input_active(&protocol));
    request[11] = 0;

    request[2] = 0x80;
    accept_active_request(&protocol, request);
    accept_active_request(&protocol, request);
    accept_active_request(&protocol, request);
    assert(wheel_protocol_acknowledgement_input_active(&protocol));
}

static void test_captures_mode_four_requests(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    wheel_protocol_set_axis_processing(&protocol, 6, WHEEL_AXIS_OVERRIDE_MODE_NONE, 0);
    protocol.mode = 4;
    protocol.phase = WHEEL_PROTOCOL_ACTIVE;
    request[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    request[2] = 0x80;
    request[3] = 0x40;
    request[4] = 0x20;
    request[8] = 0xff;
    request[9] = 0xff;
    request[10] = 0xff;
    request[11] = 0xff;
    request[12] = 0x61;
    request[18] = 0x34;
    request[19] = 0x12;
    request[31] = 0x74;
    request[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;

    accept_active_request(&protocol, request);
    accept_active_request(&protocol, request);
    assert(wheel_protocol_mode_four_input(&protocol)->buttons[0] == 0);
    accept_active_request(&protocol, request);

    const WheelPacketModeFourInput *input = wheel_protocol_mode_four_input(&protocol);
    assert(input != 0);
    assert(input->buttons[0] == 0x80);
    assert(input->buttons[1] == 0x40);
    assert(input->buttons[2] == 0x20);
    assert(input->axis_values[0] == 0x1234);
    assert(input->control_data[0] == 0x61);
    assert(input->axis_limit == 0x74);
    assert(wheel_protocol_acknowledgement_input_active(&protocol));
    assert(wheel_protocol_request_changed(&protocol));
    assert(!wheel_protocol_request_changed(&protocol));

    accept_active_request(&protocol, request);
    assert(input->controls[0] == 0x38);
    assert(input->controls[1] == 0x80);
    assert(input->controls[2] == 0);
    assert(input->controls[3] == 0);
}

static void test_builds_mode_four_active_response(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    const WheelPacketModeFourOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .display_state = {0x44, 0x55},
        .legacy_axes = {0x66, 0x77},
    };
    wheel_protocol_init(&protocol);
    wheel_protocol_set_mode_four_output(&protocol, &output);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 4);

    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    const uint8_t *response = wheel_protocol_response(&protocol);
    const uint8_t expected[WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };
    assert(memcmp(response, expected, sizeof(expected)) == 0);
    assert(wheel_protocol_message_valid(response));
}

static void test_applies_authenticated_axis_overrides(void) {
    WheelProtocol protocol;
    wheel_protocol_init(&protocol);
    wheel_protocol_set_axis_processing(&protocol, 0, WHEEL_AXIS_OVERRIDE_MODE_SECONDARY, 0);
    protocol.mode = 0x13;
    protocol.phase = WHEEL_PROTOCOL_ACTIVE;

    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    request[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    request[5] = 0x31;
    request[6] = 0xc2;
    request[10] = 1;
    request[12] = 0x30;
    request[13] = 0x60;
    request[WHEEL_PROTOCOL_FLAGS_OFFSET] = WHEEL_PROTOCOL_REQUEST_READY;
    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    const WheelPacketModeOneInput *decoded = wheel_protocol_mode_one_input(&protocol);
    assert(decoded->axis_outputs[0] == 0x4e);
    assert(decoded->axis_outputs[1] == 0x42);
    const WheelAxisOverrideProcessor *axes = wheel_protocol_axis_overrides(&protocol);
    assert(axes->overrides.axis_7.enabled);
    assert(axes->overrides.axis_7.value == 0x10);
    assert(axes->overrides.auxiliary.enabled);
    assert(axes->overrides.auxiliary.value == 0x20);
}

static void test_builds_mode_one_active_response(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    const WheelPacketModeOneOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .display_state = {0x44, 0x55},
        .link_status = {0x66, 0x77},
    };
    wheel_protocol_init(&protocol);
    wheel_protocol_set_mode_one_output(&protocol, &output);
    synchronize(&protocol, request);
    select_mode(&protocol, request, 1);

    request[WHEEL_PROTOCOL_CHECKSUM_OFFSET] = wheel_protocol_message_checksum(request);
    wheel_protocol_accept(&protocol, request);

    const uint8_t *response = wheel_protocol_response(&protocol);
    const uint8_t expected[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };
    assert(memcmp(response, expected, sizeof(expected)) == 0);
    assert(wheel_protocol_message_valid(response));
}

static void test_rejects_out_of_range_mode(void) {
    WheelProtocol protocol;
    uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    wheel_protocol_init(&protocol);
    synchronize(&protocol, request);
    select_mode(&protocol, request, WHEEL_MODE_MAXIMUM + 1);

    assert(protocol.phase == WHEEL_PROTOCOL_UNSUPPORTED);
    assert(protocol.mode == WHEEL_MODE_UNKNOWN);
}

static void test_crc8_vectors(void) {
    uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE] = {0};
    packet[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    assert(wheel_protocol_message_checksum(packet) == 0x9a);

    for (uint8_t index = 0; index < WHEEL_PROTOCOL_CONTENT_SIZE; index++) {
        packet[index] = index;
    }
    assert(wheel_protocol_message_checksum(packet) == 0x21);
}

int main(void) {
    test_synchronizes_and_selects_mode();
    test_selects_scan_variants();
    test_selects_authentication_from_wheel_mode();
    test_authentication_mode_set();
    test_selects_authentication_keys_for_each_wheel_mode();
    test_completes_encrypted_authentication_exchange();
    test_retries_invalid_authentication_challenge();
    test_retries_invalid_encrypted_proof();
    test_echoes_token_from_wrong_encrypted_command();
    test_accepts_authenticated_active_commands_and_restarts_authentication();
    test_refreshes_active_response_after_invalid_checksum();
    test_restarts_synchronization_when_ready_drops();
    test_captures_normalized_active_requests();
    test_averages_control_axes_only_for_authenticated_wheel_modes();
    test_tracks_display_acknowledgement_input();
    test_captures_mode_four_requests();
    test_applies_authenticated_axis_overrides();
    test_builds_mode_one_active_response();
    test_builds_mode_four_active_response();
    test_rejects_out_of_range_mode();
    test_crc8_vectors();
    return 0;
}
