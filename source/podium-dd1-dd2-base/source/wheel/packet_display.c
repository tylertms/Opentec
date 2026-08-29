#include "wheel/packet_display.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_DISPLAY_MODE = 0x10,
};

/**
 * @brief Reports whether a wheel mode uses the standard display packet codec.
 *
 * Selects operating mode 0x10.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x10; otherwise false.
 */
bool wheel_packet_display_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_DISPLAY_MODE;
}

/**
 * @brief Clears the display-packet input history.
 *
 * Zeros all button and control samples and resets the insertion position.
 *
 * @param[out] filter Three-sample display-packet filter to initialize.
 */
void wheel_packet_display_filter_init(WheelPacketDisplayFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_DISPLAY_HISTORY_DEPTH; sample++) {
        for (uint8_t field = 0; field < WHEEL_PACKET_DISPLAY_FILTER_WIDTH; field++) {
            filter->samples[sample][field] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * @brief Filters one standard display-packet sample.
 *
 * Keeps button bits and the first three control-byte bits present in all three recent samples,
 * then advances the shared insertion position.
 *
 * @param[in,out] filter Shared button and control history.
 * @param[in,out] input Input added to the history and filtered in place.
 */
void wheel_packet_display_filter(WheelPacketDisplayFilter *filter, WheelPacketDisplayInput *input) {
    uint8_t fields[WHEEL_PACKET_DISPLAY_FILTER_WIDTH] = {
        input->buttons[0],  input->buttons[1],  input->buttons[2],
        input->controls[0], input->controls[1], input->controls[2],
    };
    for (uint8_t field = 0; field < WHEEL_PACKET_DISPLAY_FILTER_WIDTH; field++) {
        filter->samples[filter->next_sample][field] = fields[field];
        fields[field] =
            filter->samples[0][field] & filter->samples[1][field] & filter->samples[2][field];
    }
    input->buttons[0] = fields[0];
    input->buttons[1] = fields[1];
    input->buttons[2] = fields[2];
    input->controls[0] = fields[3];
    input->controls[1] = fields[4];
    input->controls[2] = fields[5];
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_DISPLAY_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}
