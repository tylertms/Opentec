#include "wheel/packet_adapter.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal adapter-response command and display cadence. */
enum {
    WHEEL_PACKET_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6, /**< Authenticated response command. */
    DISPLAY_REFRESH_MS = 50, /**< Minimum display refresh interval in milliseconds. */
};

/**
 * @brief Reads one bit from an adapter button byte.
 *
 * Shifts the selected bit into the low position and returns it as zero or one.
 *
 * @param[in] value Source byte.
 * @param[in] bit Zero-based bit position.
 * @return Zero or one from the selected position.
 */
static uint8_t read_bit(uint8_t value, uint8_t bit) { return (value >> bit) & 1u; }

/**
 * @brief Merges one adapter button into a wheel button byte.
 *
 * Sets the selected destination bit when the source low bit is one and preserves it otherwise.
 *
 * @param[in,out] value Destination button byte.
 * @param[in] destination_bit Zero-based destination position.
 * @param[in] source Value whose low bit is merged.
 */
static void merge_bit(uint8_t *value, uint8_t destination_bit, uint8_t source) {
    *value |= (uint8_t)((source & 1u) << destination_bit);
}

/**
 * @brief Reports whether a wheel mode uses adapter-oriented packets.
 *
 * Selects authenticated mode 0x0C.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x0C.
 */
bool wheel_packet_adapter_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_ADAPTER_MODE;
}

/**
 * @brief Merges attached-adapter state into mode-0x0C input.
 *
 * Adds the adapter's mapped buttons, replaces both output axes, publishes current button activity,
 * and consumes one queued primary motion step. The primary step is published as signed wire motion
 * -1, 0, or 1. A disconnected adapter leaves the wheel input unchanged.
 *
 * @param[in,out] input Filtered wheel input receiving adapter values.
 * @param[in,out] adapter Adapter state whose queued motion is consumed.
 */
void wheel_packet_adapter_merge(WheelPacketAdapterInput *input, WheelAdapterInput *adapter) {
    if (adapter == 0 || !adapter->connected) {
        return;
    }

    input->buttons[0] |= adapter->buttons[0] & 0x0fu;
    merge_bit(&input->buttons[2], 1, read_bit(adapter->buttons[2], 3));
    merge_bit(&input->buttons[2], 5, read_bit(adapter->buttons[2], 4));
    if (adapter->mode != 1) {
        merge_bit(&input->buttons[2], 2, read_bit(adapter->buttons[2], 2));
        merge_bit(&input->buttons[1], 6, read_bit(adapter->buttons[1], 0));
        merge_bit(&input->buttons[0], 6, read_bit(adapter->buttons[1], 1));
    }
    merge_bit(&input->buttons[1], 7, read_bit(adapter->buttons[1], 5));
    input->axis_outputs[0] = adapter->axes[1];
    input->axis_outputs[1] = (uint8_t)~adapter->axes[0];
    adapter->buttons_active =
        adapter->buttons[0] != 0 || adapter->buttons[1] != 0 || adapter->buttons[2] != 0;
    if (adapter->primary_delta > 0) {
        input->motion = 1;
        adapter->primary_delta--;
    } else if (adapter->primary_delta < 0) {
        input->motion = -1;
        adapter->primary_delta++;
    } else {
        input->motion = 0;
    }
}

/**
 * @brief Encodes an adapter-oriented attached-wheel response.
 *
 * Writes command A6 and the last published display fields on every exchange. After the strict
 * 50-millisecond refresh deadline, a connected adapter without the high profile flag publishes the
 * current three display glyphs and the low and high display-report bytes. A changed nonzero display
 * report latches a pending adapter update.
 *
 * @param[in,out] output Display values, cadence, prior report, and update latch.
 * @param[in] adapter Current attached-adapter state.
 * @param[in] now_ms Current monotonic millisecond count.
 * @param[out] response Seven-byte response destination.
 */
void wheel_packet_adapter_encode(WheelPacketAdapterOutput *output, const WheelAdapterInput *adapter,
                                 uint32_t now_ms,
                                 uint8_t response[WHEEL_PACKET_ADAPTER_RESPONSE_SIZE]) {
    response[0] = WHEEL_PACKET_COMMAND_SELECT_MODE;
    response[1] = WHEEL_PACKET_COMMAND_AUTHENTICATE;
    if (now_ms > output->refresh_after_ms && adapter != 0 && adapter->connected &&
        (adapter->profile_flags & 0x80u) == 0) {
        for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
            output->published_glyphs[index] = output->display.glyphs[index];
        }
        output->published_display_report = output->display_report;
        if (output->display_report != 0 &&
            output->display_report != output->previous_display_report) {
            output->previous_display_report = output->display_report;
            output->display_update_pending = true;
        }
        output->refresh_after_ms = now_ms + DISPLAY_REFRESH_MS;
    }
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = output->published_glyphs[index];
    }
    response[5] = (uint8_t)output->published_display_report;
    response[6] = (uint8_t)(output->published_display_report >> 8);
}
