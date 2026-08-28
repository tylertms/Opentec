#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "pedal/protocol.h"

static void test_selects_protocol(void) {
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0x15) == PEDAL_PROTOCOL_V3);
    assert(pedal_protocol_select(PEDAL_DEVICE_V4, 0x26) == PEDAL_PROTOCOL_V4);
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0x14) == PEDAL_PROTOCOL_LEGACY);
    assert(pedal_protocol_select(PEDAL_DEVICE_V4, 0x25) == PEDAL_PROTOCOL_LEGACY);
    assert(pedal_protocol_select(PEDAL_DEVICE_NONE, 0x15) == PEDAL_PROTOCOL_REDISCOVER);
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0) == PEDAL_PROTOCOL_REDISCOVER);
    assert(pedal_protocol_select(PEDAL_DEVICE_INVALID, 0x15) == PEDAL_PROTOCOL_REDISCOVER);
    assert(pedal_protocol_select(PEDAL_DEVICE_V3, 0xff) == PEDAL_PROTOCOL_REDISCOVER);
}

static void test_builds_legacy_requests(void) {
    assert(pedal_legacy_request(PEDAL_LEGACY_AXIS_1, 0x7f, 0x3a) == 0x40);
    assert(pedal_legacy_request(PEDAL_LEGACY_AXIS_2, 0x7f, 0x3a) == 0xbf);
    assert(pedal_legacy_request(PEDAL_LEGACY_AXIS_3, 0x7f, 0x3a) == 0xfa);
    assert(pedal_legacy_request(PEDAL_LEGACY_AUXILIARY, 0x7f, 0x3a) == 0);
}

static void test_applies_legacy_responses(void) {
    PedalInput input = {
        .axes = {1, 2, 3},
        .auxiliary = 4,
    };

    pedal_legacy_apply_response(PEDAL_LEGACY_AXIS_1, 0xa5, false, &input);
    pedal_legacy_apply_response(PEDAL_LEGACY_AXIS_2, 0x00, false, &input);
    pedal_legacy_apply_response(PEDAL_LEGACY_AXIS_3, 0xff, false, &input);
    assert(input.axes[0] == 0x5a00);
    assert(input.axes[1] == 0xff00);
    assert(input.axes[2] == 0);

    pedal_legacy_apply_response(PEDAL_LEGACY_AUXILIARY, 0x35, true, &input);
    assert(input.auxiliary == 4);
    pedal_legacy_apply_response(PEDAL_LEGACY_AUXILIARY, 0x35, false, &input);
    assert(input.auxiliary == 0x35);
}

int main(void) {
    test_selects_protocol();
    test_builds_legacy_requests();
    test_applies_legacy_responses();
    return 0;
}
