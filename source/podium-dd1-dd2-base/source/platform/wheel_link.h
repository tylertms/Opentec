#ifndef OPENTEC_BASE_PLATFORM_WHEEL_LINK_H
#define OPENTEC_BASE_PLATFORM_WHEEL_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/transport_frame.h"

void platform_wheel_link_init(void);
void platform_wheel_link_reset(void);
bool platform_wheel_link_start(const uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]);
bool platform_wheel_link_take_received(uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]);

#endif
