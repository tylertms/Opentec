#include <assert.h>
#include <stdint.h>

#include "wheel/status.h"

static void test_decodes_status_snapshot(void) {
    const WheelTransportFrame response = {
        .length = WHEEL_STATUS_RESPONSE_SIZE,
        .data = {0x12, 0x34, 0x78, 0x56, 1, 2, 3, 4, 5, 6, 7, 8, 0x9a, 0, 0xaa},
    };
    WheelStatus status;

    assert(wheel_status_decode(&status, &response));
    assert(status.status_high == 0x12);
    assert(status.status_low == 0x34);
    assert(status.accessory_value == 0x5678);
    assert(status.runtime_seconds == UINT32_C(0x04030201));
    assert(status.runtime_counter == UINT32_C(0x08070605));
    assert(status.trailing_status == 0x9a);
    assert(status.marker_acknowledged);
}

static void test_rejects_short_status(void) {
    const WheelTransportFrame response = {
        .length = WHEEL_STATUS_RESPONSE_SIZE - 1,
    };
    WheelStatus status;
    assert(!wheel_status_decode(&status, &response));
}

int main(void) {
    test_decodes_status_snapshot();
    test_rejects_short_status();
    return 0;
}
