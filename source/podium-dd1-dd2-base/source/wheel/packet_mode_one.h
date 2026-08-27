#ifndef OPENTEC_BASE_WHEEL_PACKET_MODE_ONE_H
#define OPENTEC_BASE_WHEEL_PACKET_MODE_ONE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

enum { WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE = 9 };

typedef struct {
    WheelDisplayOutput display;
    uint8_t operating_mode;
    uint8_t display_state[2];
    uint8_t link_status[2];
} WheelPacketModeOneOutput;

bool wheel_packet_mode_one_applies(uint8_t wheel_mode);
void wheel_packet_mode_one_encode(const WheelPacketModeOneOutput *output,
                                  uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE]);

#endif
