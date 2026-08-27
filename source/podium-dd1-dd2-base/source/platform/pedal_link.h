#ifndef OPENTEC_BASE_PLATFORM_PEDAL_LINK_H
#define OPENTEC_BASE_PLATFORM_PEDAL_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"

void platform_pedal_link_init(void);
void platform_pedal_link_begin_analog(void);
void platform_pedal_link_begin_discovery(void);
void platform_pedal_link_begin_framed_receive(void);
bool platform_pedal_link_send_byte(uint8_t value);
bool platform_pedal_link_send_frame(const uint8_t frame[PEDAL_FRAME_SIZE]);
bool platform_pedal_link_take_byte(uint8_t *value);
bool platform_pedal_link_take_frame(uint8_t frame[PEDAL_FRAME_SIZE]);

#endif
