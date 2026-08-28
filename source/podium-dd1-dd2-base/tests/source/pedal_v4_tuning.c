#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pedal/v4_tuning.h"

static void test_builds_brake_force_request(void) {
    static const uint8_t expected[PEDAL_V4_TUNING_REQUEST_SIZE] = {
        0x00, 0x00, 0x0a, 0x02, 0x00, 0x00, 0x08, 0x02, 0x00, 0x00, 0x18, 0x01,
        0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0xaa, 0x20, 0x32, 0xea, 0x2f,
    };
    uint8_t request[PEDAL_V4_TUNING_REQUEST_SIZE];

    assert(pedal_v4_tuning_request(PEDAL_V4_TUNING_BRAKE_FORCE, 50, request));
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

static void test_maps_curve_offsets(void) {
    uint8_t request[PEDAL_V4_TUNING_REQUEST_SIZE];

    assert(pedal_v4_tuning_request(PEDAL_V4_TUNING_THROTTLE_CURVE, 1, request));
    assert(request[19] == 8);
    assert(pedal_v4_tuning_request(PEDAL_V4_TUNING_BRAKE_CURVE, 2, request));
    assert(request[19] == 16);
    assert(pedal_v4_tuning_request(PEDAL_V4_TUNING_CLUTCH_CURVE, 3, request));
    assert(request[19] == 24);
}

static void test_rejects_unknown_setting(void) {
    uint8_t request[PEDAL_V4_TUNING_REQUEST_SIZE];
    memset(request, 0xa5, sizeof(request));

    assert(!pedal_v4_tuning_request((PedalV4TuningSetting)0, 10, request));
    for (uint8_t index = 0; index < sizeof(request); index++) {
        assert(request[index] == 0xa5);
    }
}

int main(void) {
    test_builds_brake_force_request();
    test_maps_curve_offsets();
    test_rejects_unknown_setting();
    return 0;
}
