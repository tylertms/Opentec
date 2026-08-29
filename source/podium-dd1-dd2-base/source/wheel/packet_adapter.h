#ifndef OPENTEC_BASE_WHEEL_PACKET_ADAPTER_H
#define OPENTEC_BASE_WHEEL_PACKET_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/adapter.h"
#include "wheel/display_output.h"
#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_ADAPTER_MODE = 0x0c,
    WHEEL_PACKET_ADAPTER_RESPONSE_SIZE = 7,
};

/** @brief Logical input carried by the adapter-oriented mode-0x0C packet. */
typedef WheelPacketCommonInput WheelPacketAdapterInput;

/** @brief Display values and refresh state for adapter-oriented responses. */
typedef struct {
    WheelDisplayOutput display;
    uint8_t published_glyphs[WHEEL_DISPLAY_GLYPH_COUNT];
    uint32_t refresh_after_ms;
    uint16_t display_report;
    uint16_t published_display_report;
    uint16_t previous_display_report;
    bool display_update_pending;
} WheelPacketAdapterOutput;

bool wheel_packet_adapter_applies(uint8_t wheel_mode);
void wheel_packet_adapter_merge(WheelPacketAdapterInput *input, WheelAdapterInput *adapter);
void wheel_packet_adapter_encode(WheelPacketAdapterOutput *output, const WheelAdapterInput *adapter,
                                 uint32_t now_ms,
                                 uint8_t response[WHEEL_PACKET_ADAPTER_RESPONSE_SIZE]);

#endif
