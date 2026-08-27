#include "wheel/packet_mode_one.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,
    WHEEL_PACKET_AUTHENTICATION_MODE_FIRST = 0x13,
    WHEEL_PACKET_AUTHENTICATION_MODE_LAST = 0x14,
};

/**
 * Tests whether an attached-wheel mode uses the mode-1 packet codec.
 *
 * @param wheel_mode Selected attached-wheel mode.
 * @return True for modes 1, 3, 0x13, and 0x14; otherwise false.
 */
bool wheel_packet_mode_one_applies(uint8_t wheel_mode) {
    return wheel_mode == 1 || wheel_mode == 3 || wheel_mode == 0x13 || wheel_mode == 0x14;
}

/**
 * Encodes the shared nine-byte output used by attached-wheel modes 1, 3, 0x13, and 0x14.
 *
 * @param output Current operating mode, display output, display state, and link status.
 * @param response Nine-byte destination buffer.
 */
void wheel_packet_mode_one_encode(const WheelPacketModeOneOutput *output,
                                  uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE]) {
    response[0] = output->operating_mode >= WHEEL_PACKET_AUTHENTICATION_MODE_FIRST &&
                          output->operating_mode <= WHEEL_PACKET_AUTHENTICATION_MODE_LAST
                      ? WHEEL_PACKET_COMMAND_AUTHENTICATE
                      : WHEEL_PACKET_COMMAND_SELECT_MODE;
    response[1] = 0;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = output->display.glyphs[index];
    }
    if (output->display.third_glyph_marker) {
        response[4] |= 0x80u;
    }
    response[5] = output->display_state[0];
    response[6] = output->display_state[1];
    response[7] = output->link_status[0];
    response[8] = output->link_status[1];
}
