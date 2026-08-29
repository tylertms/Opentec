#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_metadata.h"

static void test_selects_metadata_packet_mode(void) {
    for (uint16_t mode = 0; mode <= UINT8_MAX; mode++) {
        assert(wheel_packet_metadata_applies((uint8_t)mode) ==
               (mode == WHEEL_PACKET_METADATA_MODE));
    }
}

static void test_decodes_only_report_metadata(void) {
    uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE];
    for (uint8_t index = 0; index < sizeof(request); index++) {
        request[index] = index;
    }
    WheelPacketMetadataInput input;
    wheel_packet_metadata_decode(request, &input);

    assert(input.axis_values[0] == 0x1312);
    assert(input.axis_values[1] == 0x1514);
    assert(input.report_mode == 0x1c);
    assert(input.report_capabilities == 0x1e);
    assert(input.axis_limit == 0x1f);
    assert(input.buttons[0] == 0);
    assert(input.motion == 0);
    assert(input.controls[0] == 0);
    assert(input.axis_outputs[0] == 0);
}

int main(void) {
    test_selects_metadata_packet_mode();
    test_decodes_only_report_metadata();
    return 0;
}
