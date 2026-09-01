#include "wheel.h"

#include <stddef.h>
#include <string.h>

enum {
    WHEEL_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_MODE = 1,
    WHEEL_CONTENT_SIZE = 32,
    WHEEL_CHECKSUM_OFFSET = 32,
    WHEEL_BUTTON_OFFSET = 2,
    WHEEL_HANDSHAKE_EXCHANGES = 3,
};

uint8_t dd1_sim_wheel_checksum(const uint8_t packet[DD1_SIM_WHEEL_PACKET_SIZE]) {
    uint8_t checksum = UINT8_MAX;
    for (size_t index = 0; index < WHEEL_CONTENT_SIZE; ++index) {
        checksum ^= packet[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            checksum = (checksum & 1U) != 0 ? (uint8_t)((checksum >> 1U) ^ 0x8cU)
                                            : (uint8_t)(checksum >> 1U);
        }
    }
    return checksum;
}

void dd1_sim_wheel_init(Dd1SimWheel *wheel) { memset(wheel, 0, sizeof(*wheel)); }

void dd1_sim_wheel_set_buttons(Dd1SimWheel *wheel, uint32_t buttons) {
    wheel->buttons = buttons & UINT32_C(0x00ffffff);
}

void dd1_sim_wheel_response(const Dd1SimWheel *wheel, uint8_t response[DD1_SIM_WHEEL_PACKET_SIZE]) {
    memset(response, 0, DD1_SIM_WHEEL_PACKET_SIZE);
    if (wheel->exchanges >= WHEEL_HANDSHAKE_EXCHANGES) {
        response[0] = WHEEL_COMMAND_SELECT_MODE;
        response[1] = WHEEL_MODE;
    }
    if (wheel->exchanges > WHEEL_HANDSHAKE_EXCHANGES) {
        response[WHEEL_BUTTON_OFFSET] = (uint8_t)wheel->buttons;
        response[WHEEL_BUTTON_OFFSET + 1] = (uint8_t)(wheel->buttons >> 8U);
        response[WHEEL_BUTTON_OFFSET + 2] = (uint8_t)(wheel->buttons >> 16U);
    }
    response[WHEEL_CHECKSUM_OFFSET] = dd1_sim_wheel_checksum(response);
}

void dd1_sim_wheel_accept_output(Dd1SimWheel *wheel,
                                 const uint8_t output[DD1_SIM_WHEEL_OUTPUT_SIZE]) {
    memcpy(wheel->output, output, sizeof(wheel->output));
    ++wheel->exchanges;
}
