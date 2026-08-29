#include "wheel/packet_remapped.h"

#include <stdint.h>

enum {
    WHEEL_PACKET_REMAPPED_MODE = 0x11,
    INTERFACE_MODE_PLAYSTATION_4 = 7,
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
 * @brief Reports whether a wheel mode uses the remapped packet policy.
 *
 * Selects operating mode 0x11.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x11; otherwise false.
 */
bool wheel_packet_remapped_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_REMAPPED_MODE;
}

/**
 * @brief Clears the remapped-packet button history.
 *
 * Zeros all three button samples and resets the insertion position.
 *
 * @param[out] filter Three-sample button filter to initialize.
 */
void wheel_packet_remapped_filter_init(WheelPacketRemappedFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_REMAPPED_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
            filter->samples[sample][button] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * @brief Remaps and filters one remapped-packet button sample.
 *
 * In PlayStation mode, moves the third button byte's second bit into the first byte's fifth bit,
 * mirrors its first bit into its second bit, and exchanges the second byte's first and fourth bits.
 * The resulting buttons retain only bits present in all three recent samples.
 *
 * @param[in,out] filter Three-sample button history.
 * @param[in,out] input Input whose buttons are remapped and filtered in place.
 * @param[in] interface_mode Active host interface mode.
 */
void wheel_packet_remapped_filter(WheelPacketRemappedFilter *filter,
                                  WheelPacketRemappedInput *input, uint8_t interface_mode) {
    uint8_t *sample = filter->samples[filter->next_sample];
    for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
        sample[button] = input->buttons[button];
    }
    if (interface_mode == INTERFACE_MODE_PLAYSTATION_4) {
        assign_bit(&sample[0], 4, input->buttons[2] & 0x02u);
        assign_bit(&sample[2], 1, input->buttons[2] & 0x01u);
        assign_bit(&sample[1], 3, input->buttons[1] & 0x01u);
        assign_bit(&sample[1], 0, input->buttons[1] & 0x08u);
    }
    for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
        input->buttons[button] =
            filter->samples[0][button] & filter->samples[1][button] & filter->samples[2][button];
    }
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_REMAPPED_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * @brief Decodes the remapped packet's primary motion flags.
 *
 * Gives the positive flag priority over the negative flag and treats every other combination as
 * idle.
 *
 * @param[in] input Remapped packet input containing the motion flags.
 * @return Positive one, negative one, or zero.
 */
int8_t wheel_packet_remapped_primary_delta(const WheelPacketRemappedInput *input) {
    if (((uint8_t)input->motion & 0x10u) != 0) {
        return 1;
    }
    return ((uint8_t)input->motion & 0x20u) != 0 ? -1 : 0;
}
