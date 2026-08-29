#include "wheel/packet_alternate.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    WHEEL_PACKET_ALTERNATE_MODE = 0x12,
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,
    INTERFACE_MODE_XBOX_GIP = 6,
    INTERFACE_MODE_PLAYSTATION_4 = 7,
    TRANSFER_SEQUENCE_LAST = 29,
};

/**
 * @brief Assigns one button bit from a normalized Boolean value.
 *
 * Replaces the selected destination bit and preserves every other bit.
 *
 * @param[in,out] destination Button byte to update.
 * @param[in] bit Zero-based destination bit.
 * @param[in] value Zero to clear the bit; nonzero to set it.
 */
static void assign_bit(uint8_t *destination, uint8_t bit, uint8_t value) {
    uint8_t mask = (uint8_t)(1u << bit);
    *destination = (uint8_t)((*destination & (uint8_t)~mask) | (value != 0 ? mask : 0));
}

/**
 * @brief Reports whether a wheel mode uses the alternate packet policy.
 *
 * Selects operating mode 0x12.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x12; otherwise false.
 */
bool wheel_packet_alternate_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_ALTERNATE_MODE;
}

/**
 * @brief Clears the alternate-packet button history.
 *
 * Zeros all three button samples and resets the insertion position.
 *
 * @param[out] filter Three-sample button filter to initialize.
 */
void wheel_packet_alternate_filter_init(WheelPacketAlternateFilter *filter) {
    memset(filter, 0, sizeof(*filter));
}

/**
 * @brief Remaps and filters one alternate-packet input sample.
 *
 * Exchanges the second button byte's first and fourth bits for every interface. Xbox GIP moves
 * the third byte's second bit into its third bit. Other interfaces move the third byte's first
 * bit into its fourth bit and clear its source, while PlayStation also mirrors that source into
 * the second bit and merges the original second bit into the first byte's fifth bit. Buttons
 * retain only bits present in all three recent samples, and control bytes two through five clear.
 *
 * @param[in,out] filter Three-sample button history.
 * @param[in,out] input Input whose buttons and controls are normalized in place.
 * @param[in] interface_mode Active host interface mode.
 */
void wheel_packet_alternate_filter(WheelPacketAlternateFilter *filter,
                                   WheelPacketAlternateInput *input, uint8_t interface_mode) {
    uint8_t button_zero = input->buttons[0];
    uint8_t button_one = input->buttons[1];
    uint8_t button_two = input->buttons[2];
    uint8_t *sample = filter->samples[filter->next_sample];
    sample[0] = button_zero;
    sample[1] = button_one;
    sample[2] = button_two;
    assign_bit(&sample[1], 0, button_one & 0x08u);
    assign_bit(&sample[1], 3, button_one & 0x01u);

    if (interface_mode == INTERFACE_MODE_XBOX_GIP) {
        assign_bit(&sample[2], 2, button_two & 0x02u);
    } else {
        assign_bit(&sample[2], 3, button_two & 0x01u);
        if (interface_mode == INTERFACE_MODE_PLAYSTATION_4) {
            assign_bit(&sample[2], 1, button_two & 0x01u);
            assign_bit(&sample[0], 4, (uint8_t)((button_two & 0x02u) | (button_zero & 0x10u)));
        } else {
            assign_bit(&sample[2], 0, 0);
        }
    }

    for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
        input->buttons[button] =
            filter->samples[0][button] & filter->samples[1][button] & filter->samples[2][button];
    }
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_ALTERNATE_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
    for (uint8_t control = 2; control <= 5; control++) {
        input->controls[control] = 0;
    }
}

/**
 * @brief Queues one alternate-packet transfer payload.
 *
 * Retains all 30 bytes and restarts the transfer sequence when no earlier payload is pending.
 *
 * @param[in,out] output Retained alternate-packet output state.
 * @param[in] payload Complete 30-byte transfer payload.
 * @return True when the payload was retained.
 */
bool wheel_packet_alternate_queue_payload(
    WheelPacketAlternateOutput *output,
    const uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE]) {
    if (output == NULL || payload == NULL || output->payload_pending) {
        return false;
    }
    memcpy(output->payload, payload, sizeof(output->payload));
    output->sequence = 0;
    output->payload_pending = true;
    return true;
}

/**
 * @brief Reports whether an alternate transfer payload is pending.
 *
 * Tests the retained transfer state without advancing its sequence.
 *
 * @param[in] output Alternate-packet output state.
 * @return True while a queued payload awaits completion.
 */
bool wheel_packet_alternate_payload_pending(const WheelPacketAlternateOutput *output) {
    return output != NULL && output->payload_pending;
}

/**
 * @brief Encodes an alternate transfer frame when its cadence is due.
 *
 * Advances one sequence position while a queued or continuously active transfer is eligible.
 * Every fourth position emits command 0x12 with the retained 30-byte payload. The terminal
 * position emits once more, clears queued state, and restarts the sequence. Suppressed transfers
 * do not advance.
 *
 * @param[in,out] output Alternate-packet output and transfer state.
 * @param[out] response Response receiving a transfer frame when due.
 * @return True when a transfer frame was encoded.
 */
static bool encode_transfer(WheelPacketAlternateOutput *output, uint8_t *response) {
    if ((!output->payload_pending && !output->transfer_active) || output->payload_suppressed) {
        return false;
    }

    uint8_t previous_sequence = output->sequence++;
    bool terminal = previous_sequence > TRANSFER_SEQUENCE_LAST;
    if (terminal) {
        output->sequence = 0;
        output->payload_pending = false;
    } else if ((output->sequence & 3u) != 0) {
        return false;
    }

    response[0] = WHEEL_PACKET_COMMAND_AUTHENTICATE;
    response[1] = WHEEL_PACKET_ALTERNATE_MODE;
    memcpy(response + 2, output->payload, sizeof(output->payload));
    return true;
}

/**
 * @brief Encodes the alternate packet's default display response.
 *
 * Writes command A6, auxiliary-link option, three display glyphs, auxiliary display state,
 * vendor-command endpoint state, and a one-shot motor-link restart marker. Suppressed auxiliary
 * display output clears both auxiliary bytes.
 *
 * @param[in,out] output Alternate-packet output state and one-shot restart marker.
 * @param[out] response Response receiving the default fields.
 */
static void encode_default(WheelPacketAlternateOutput *output, uint8_t *response) {
    memset(response, 0, WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE);
    response[0] = WHEEL_PACKET_COMMAND_AUTHENTICATE;
    if (output->auxiliary_link_option != 0) {
        response[1] = 2;
    }
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = output->display.glyphs[index];
    }
    if (!output->suppress_auxiliary_display) {
        response[5] = output->display.auxiliary;
        response[6] = (uint8_t)((output->auxiliary_status ? 1u : 0u) |
                                (output->display.third_glyph_marker ? 2u : 0u));
    }
    response[9] = output->report_state;
    response[10] = output->command_restart_pending ? UINT8_MAX : 0;
    output->command_restart_pending = false;
}

/**
 * @brief Encodes the next alternate-packet response.
 *
 * Emits a due transfer frame or the default display response. Transfer scheduling takes priority,
 * and a default response consumes the one-shot motor-link restart marker.
 *
 * @param[in,out] output Alternate-packet output and transfer state.
 * @param[out] response Thirty-three-byte response content destination.
 */
void wheel_packet_alternate_encode(WheelPacketAlternateOutput *output,
                                   uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE]) {
    if (!encode_transfer(output, response)) {
        encode_default(output, response);
    }
}
