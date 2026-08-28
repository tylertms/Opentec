#ifndef OPENTEC_BASE_PLATFORM_PEDAL_LINK_H
#define OPENTEC_BASE_PLATFORM_PEDAL_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"
#include "transfer/frame.h"

void platform_pedal_link_init(void);
void platform_pedal_link_begin_analog(void);
void platform_pedal_link_begin_discovery(void);
void platform_pedal_link_begin_framed_receive(void);
void platform_pedal_link_begin_transfer_receive(void);
bool platform_pedal_link_send_byte(uint8_t value);
bool platform_pedal_link_send_frame(const uint8_t frame[PEDAL_FRAME_SIZE]);
bool platform_pedal_link_send_transfer(const uint8_t *data, uint16_t length);
bool platform_pedal_link_transmit_busy(void);
bool platform_pedal_link_take_byte(uint8_t *value);
bool platform_pedal_link_take_frame(uint8_t frame[PEDAL_FRAME_SIZE]);
uint16_t platform_pedal_link_take_transfer(uint8_t *data, uint16_t capacity);

#endif
