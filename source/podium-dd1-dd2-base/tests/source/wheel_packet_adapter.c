#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_adapter.h"

static void test_selects_adapter_packet_mode(void) {
    for (uint16_t mode = 0; mode <= UINT8_MAX; mode++) {
        assert(wheel_packet_adapter_applies((uint8_t)mode) == (mode == WHEEL_PACKET_ADAPTER_MODE));
    }
}

static void test_merges_adapter_input(void) {
    WheelPacketAdapterInput input = {
        .buttons = {0x10, 0x01, 0x40},
    };
    WheelAdapterInput adapter = {
        .buttons = {0x0f, 0x23, 0x1c},
        .axes = {0x12, 0x34},
        .mode = 2,
        .primary_delta = -2,
        .connected = true,
    };

    wheel_packet_adapter_merge(&input, &adapter);

    assert(input.buttons[0] == 0x5f);
    assert(input.buttons[1] == 0xc1);
    assert(input.buttons[2] == 0x66);
    assert(input.axis_outputs[0] == 0x34);
    assert(input.axis_outputs[1] == 0xed);
    assert(input.motion == -1);
    assert(adapter.primary_delta == -1);
    assert(adapter.buttons_active);
}

static void test_uses_reduced_mode_one_mapping(void) {
    WheelPacketAdapterInput input = {0};
    WheelAdapterInput adapter = {
        .buttons = {0, 0x23, 0x1c},
        .mode = 1,
        .primary_delta = 2,
        .connected = true,
    };

    wheel_packet_adapter_merge(&input, &adapter);

    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0x80);
    assert(input.buttons[2] == 0x22);
    assert(input.motion == 1);
    assert(adapter.primary_delta == 1);
}

static void test_ignores_disconnected_adapter(void) {
    WheelPacketAdapterInput input = {.buttons = {1, 2, 3}, .motion = 4};
    WheelAdapterInput adapter = {.buttons = {UINT8_MAX, UINT8_MAX, UINT8_MAX}, .primary_delta = 2};

    wheel_packet_adapter_merge(&input, &adapter);

    assert(input.buttons[0] == 1);
    assert(input.buttons[1] == 2);
    assert(input.buttons[2] == 3);
    assert(input.motion == 4);
    assert(adapter.primary_delta == 2);
}

static void test_encodes_paced_adapter_display(void) {
    WheelPacketAdapterOutput output = {
        .display = {.glyphs = {1, 2, 3}},
        .display_report = 0x5678,
    };
    WheelAdapterInput adapter = {.connected = true};
    uint8_t response[WHEEL_PACKET_ADAPTER_RESPONSE_SIZE] = {0};

    wheel_packet_adapter_encode(&output, &adapter, 0, response);
    assert(response[0] == 0xa6);
    assert(response[2] == 0);
    assert(!output.display_update_pending);

    wheel_packet_adapter_encode(&output, &adapter, 1, response);
    assert(response[2] == 1);
    assert(response[3] == 2);
    assert(response[4] == 3);
    assert(response[5] == 0x78);
    assert(response[6] == 0x56);
    assert(output.refresh_after_ms == 51);
    assert(output.previous_display_report == 0x5678);
    assert(output.display_update_pending);

    output.display_update_pending = false;
    response[2] = 0;
    wheel_packet_adapter_encode(&output, &adapter, 51, response);
    assert(response[2] == 1);
    wheel_packet_adapter_encode(&output, &adapter, 52, response);
    assert(response[2] == 1);
    assert(!output.display_update_pending);

    adapter.profile_flags = 0x80;
    response[2] = 0;
    wheel_packet_adapter_encode(&output, &adapter, 103, response);
    assert(response[2] == 1);
    assert(output.refresh_after_ms == 102);
}

int main(void) {
    test_selects_adapter_packet_mode();
    test_merges_adapter_input();
    test_uses_reduced_mode_one_mapping();
    test_ignores_disconnected_adapter();
    test_encodes_paced_adapter_display();
    return 0;
}
