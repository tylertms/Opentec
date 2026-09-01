#ifndef OPENTEC_BASE_WHEEL_PACKET_ADAPTER_H
#define OPENTEC_BASE_WHEEL_PACKET_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/adapter.h"
#include "wheel/display_output.h"
#include "wheel/packet_common.h"

/** @brief Adapter-packet mode and response dimensions. */
enum {
    WHEEL_PACKET_ADAPTER_MODE = 0x0c,       /**< Adapter-oriented wheel mode. */
    WHEEL_PACKET_ADAPTER_RESPONSE_SIZE = 7, /**< Adapter response size in bytes. */
};

/** @brief Logical input carried by the adapter-oriented mode-0x0C packet. */
typedef WheelPacketCommonInput WheelPacketAdapterInput;

/** @brief Display values and refresh state for adapter-oriented responses. */
typedef struct {
    WheelDisplayOutput display;                          /**< Current display glyph output. */
    uint8_t published_glyphs[WHEEL_DISPLAY_GLYPH_COUNT]; /**< Last published glyphs. */
    uint32_t refresh_after_ms;         /**< Earliest time for the next display refresh. */
    uint16_t display_report;           /**< Latest adapter display report. */
    uint16_t published_display_report; /**< Last display report sent to the wheel. */
    uint16_t previous_display_report;  /**< Previous display report used for change detection. */
    bool display_update_pending;       /**< Whether a changed display awaits publication. */
} WheelPacketAdapterOutput;

/**
 * @brief Reports whether a wheel mode uses adapter-oriented packets.
 *
 * Selects the authenticated adapter mode 0x0C.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for WHEEL_PACKET_ADAPTER_MODE; otherwise false.
 */
bool wheel_packet_adapter_applies(uint8_t wheel_mode);

/**
 * @brief Merges attached-adapter state into adapter-packet input.
 *
 * Maps adapter buttons and axes into the wheel input and consumes one queued primary motion step.
 * A disconnected adapter leaves the input unchanged.
 *
 * @param[in,out] input Wheel input receiving adapter values.
 * @param[in,out] adapter Adapter state whose motion and activity are updated.
 */
void wheel_packet_adapter_merge(WheelPacketAdapterInput *input, WheelAdapterInput *adapter);

/**
 * @brief Encodes an adapter-oriented attached-wheel response.
 *
 * Writes the authentication command and publishes a display refresh when its cadence and adapter
 * state permit it.
 *
 * @param[in,out] output Display and refresh state to update.
 * @param[in] adapter Current attached-adapter state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] response Seven-byte response destination.
 */
void wheel_packet_adapter_encode(WheelPacketAdapterOutput *output, const WheelAdapterInput *adapter,
                                 uint32_t now_ms,
                                 uint8_t response[WHEEL_PACKET_ADAPTER_RESPONSE_SIZE]);

#endif
