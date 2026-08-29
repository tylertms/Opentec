#include "usb/xbox_gip_response.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_advances_and_wraps_response_sequence(void) {
    uint8_t sequence = 1;
    assert(usb_xbox_gip_sequence_take(&sequence) == 1);
    assert(sequence == 2);

    sequence = 254;
    assert(usb_xbox_gip_sequence_take(&sequence) == 254);
    assert(sequence == 255);
    assert(usb_xbox_gip_sequence_take(&sequence) == 1);
    assert(sequence == 1);
}

static void test_encodes_digest_response(void) {
    static const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE] = {
        0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87,
    };
    static const uint8_t expected[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE] = {
        0x02, 0x20, 0x2a, 0x1c, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76,
        0x87, 0xb7, 0x0e, 0x50, 0x0f, 0x03, 0x00, 0x09, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    };
    uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE];

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD1, 6, 0x2a, digest, output);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_maps_base_and_extended_status_modes(void) {
    uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE] = {0};
    uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE];

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD2, 10, 1, digest, output);
    assert(output[14] == 0x64 && output[15] == 0x0f);
    assert(output[24] == 4);

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD1, 29, 1, digest, output);
    assert(output[14] == 0x53 && output[15] == 0x0f);
    assert(output[24] == 5);

    usb_xbox_gip_digest_response_encode(BOARD_VARIANT_DD1, 0, 1, digest, output);
    assert(output[14] == 0 && output[15] == 0);
    assert(output[24] == 4);
}

static void test_encodes_session_responses(void) {
    static const uint8_t expected_ready[USB_XBOX_GIP_READY_RESPONSE_SIZE] = {
        0x03, 0x20, 0x2a, 0x04, 0x80, 0x01, 0x00, 0x00,
    };
    static const uint8_t expected_status[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE] = {
        0x01, 0x20, 0x2a, 0x09, 0x02, 0x05, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    const uint8_t request[] = {5, 0x99};
    uint8_t ready[USB_XBOX_GIP_READY_RESPONSE_SIZE];
    uint8_t status[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE];

    usb_xbox_gip_ready_response_encode(0x2a, ready);
    usb_xbox_gip_transfer_status_response_encode(0x2a, request, status);
    assert(memcmp(ready, expected_ready, sizeof(expected_ready)) == 0);
    assert(memcmp(status, expected_status, sizeof(expected_status)) == 0);
}

static void test_encodes_capability_response(void) {
    static const uint8_t expected[USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE] = {
        0x21, 0x00, 0x2a, 0x33, 0x10, 0x10, 0x10, 0x10, 0x08, 0x5a, 0x00, 0x38, 0x04, 0x01, 0x48,
    };
    uint8_t output[USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE];

    usb_xbox_gip_capability_response_encode(0x2a, output);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_encodes_extended_status_response(void) {
    static const UsbXboxGipExtendedStatus status = {
        .board_variant = BOARD_VARIANT_DD1,
        .wheel_mode = 0x1d,
        .pedal_connection_flags = 0,
        .auxiliary_axis_active = 1,
        .axis_mode = 1,
        .transfer_code = 0x2a,
        .multi_position_mode = 2,
        .hardware_option = true,
        .h_pattern_available = true,
        .legacy_pedal_mode = true,
        .primary_pedal_calibration = true,
        .secondary_pedal_calibration = true,
        .pedal_recovery_handshake = true,
        .thermal_effect_limit = true,
        .wheel_calibration_available = true,
        .wheel_input_capability_available = true,
        .multi_position_supported = true,
        .adapter_connected = true,
    };
    static const uint8_t expected[USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE] = {
        0x11, 0x00, 0x2a, 0x0d, 0xff, 0x1d, 0x10, 0x01, 0x01,
        0x03, 0x09, 0x2a, 0x02, 0x01, 0x07, 0x00, 0x00,
    };
    uint8_t output[USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE];

    usb_xbox_gip_extended_status_response_encode(0x2a, &status, output);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_maps_extended_status_variants(void) {
    UsbXboxGipExtendedStatus status = {
        .board_variant = BOARD_VARIANT_DD2,
        .multi_position_supported = true,
    };
    uint8_t output[USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE];
    static const uint8_t expected_variants[] = {1, 3, 2, 0};

    for (uint8_t mode = 0; mode < sizeof(expected_variants); mode++) {
        status.multi_position_mode = mode;
        usb_xbox_gip_extended_status_response_encode(1, &status, output);
        assert(output[12] == expected_variants[mode]);
        assert(output[14] == 8);
    }
    status.multi_position_supported = false;
    usb_xbox_gip_extended_status_response_encode(1, &status, output);
    assert(output[12] == 4);

    status.board_variant = BOARD_VARIANT_DD1;
    usb_xbox_gip_extended_status_response_encode(1, &status, output);
    assert(output[14] == 6);
}

static void test_encodes_input_response(void) {
    static const UsbXboxGipInputSnapshot snapshot = {
        .buttons = {0x12, 0x34},
        .steering = 0x5678,
        .pedals = {0x9abc, 0xdef0, 0x1357},
        .auxiliary_pedal = 0x24,
        .axis_mode = 1,
        .led_state = 5,
        .steering_range_degrees = 1080,
        .force_feedback_level = 0x59,
        .pedal_active = {true, false, true},
        .auxiliary_pedal_active = true,
        .clutch_paddles = {0x68, 0x79},
        .selectors = {0x8a, 0x9b, 0xac, 0xbd, 0xce, 0xdf},
        .button_flags = 0xe1,
        .packed_buttons = 0xf2,
        .auxiliary_buttons = {0x03, 0x14, 0x25},
        .extended_button = 1,
    };
    static const uint8_t expected[USB_XBOX_GIP_INPUT_RESPONSE_SIZE] = {
        0x20, 0x00, 0x2a, 0x32, 0x12, 0x34, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde, 0x57,
        0x13, 0x24, 0x66, 0x05, 0x30, 0x2a, 0x59, 0xb8, 0x68, 0x79, 0x8a, 0x9b, 0xac,
        0xbd, 0xce, 0xdf, 0xe1, 0xf2, 0x00, 0x00, 0x03, 0x14, 0x25, 0xff, 0x01,
    };
    uint8_t output[USB_XBOX_GIP_INPUT_RESPONSE_SIZE];

    usb_xbox_gip_input_response_encode(0x2a, &snapshot, output);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

int main(void) {
    test_advances_and_wraps_response_sequence();
    test_encodes_digest_response();
    test_maps_base_and_extended_status_modes();
    test_encodes_session_responses();
    test_encodes_capability_response();
    test_encodes_extended_status_response();
    test_maps_extended_status_variants();
    test_encodes_input_response();
    return 0;
}
