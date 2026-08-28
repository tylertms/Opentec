#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pedal/v4_status.h"

static void parse_payload(const uint8_t *payload, uint16_t payload_length,
                          uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT]) {
    uint8_t response[80] = {0};
    memcpy(response + PEDAL_V4_STATUS_ENVELOPE_SIZE, payload, payload_length);
    pedal_v4_status_parse(response, PEDAL_V4_STATUS_ENVELOPE_SIZE + payload_length, axes);
}

static void test_decodes_axis_records(void) {
    const uint8_t payload[] = {
        0x0a, 0x05, 0x08, 0x01, 0x10, 0xac, 0x02, 0x0a, 0x05, 0x08, 0x02, 0x10,
        0xb4, 0x24, 0x0a, 0x08, 0x08, 0x03, 0x10, 0xff, 0xff, 0xff, 0xff, 0x0f,
    };
    uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT] = {0};

    parse_payload(payload, sizeof(payload), axes);

    assert(axes[0] == 0x1234);
    assert(axes[1] == 300);
    assert(axes[2] == UINT16_MAX);
}

static void test_skips_unknown_fields(void) {
    const uint8_t payload[] = {
        0x0a, 0x0c, 0x18, 0x80, 0x01, 0x1a, 0x03, 0xaa, 0xbb, 0xcc, 0x08, 0x02, 0x10, 0x2a,
    };
    uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT] = {1, 2, 3};

    parse_payload(payload, sizeof(payload), axes);

    assert(axes[0] == 42);
    assert(axes[1] == 0);
    assert(axes[2] == 0);
}

static void test_ignores_unknown_selectors(void) {
    const uint8_t payload[] = {
        0x0a, 0x04, 0x08, 0x00, 0x10, 0x01, 0x0a, 0x04, 0x08, 0x04, 0x10, 0x02,
    };
    uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT] = {1, 2, 3};

    parse_payload(payload, sizeof(payload), axes);

    assert(axes[0] == 0);
    assert(axes[1] == 0);
    assert(axes[2] == 0);
}

static void test_preserves_axes_without_a_payload(void) {
    uint8_t response[PEDAL_V4_STATUS_ENVELOPE_SIZE] = {0};
    uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT] = {1, 2, 3};

    pedal_v4_status_parse(response, sizeof(response), axes);
    assert(axes[0] == 1);
    assert(axes[1] == 2);
    assert(axes[2] == 3);

    pedal_v4_status_parse(NULL, UINT16_MAX, axes);
    assert(axes[0] == 1);
    assert(axes[1] == 2);
    assert(axes[2] == 3);
}

static void test_returns_partial_records_after_malformed_data(void) {
    const uint8_t payload[] = {
        0x0a, 0x04, 0x08, 0x01, 0x10, 0x2a, 0x0a, 0x05, 0x08,
    };
    uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT] = {1, 2, 3};

    parse_payload(payload, sizeof(payload), axes);

    assert(axes[0] == 0);
    assert(axes[1] == 42);
    assert(axes[2] == 0);
}

int main(void) {
    test_decodes_axis_records();
    test_skips_unknown_fields();
    test_ignores_unknown_selectors();
    test_preserves_axes_without_a_payload();
    test_returns_partial_records_after_malformed_data();
    return 0;
}
