#ifndef OPENTEC_BASE_PLATFORM_MOTOR_LINK_H
#define OPENTEC_BASE_PLATFORM_MOTOR_LINK_H

#include <stdbool.h>
#include <stdint.h>

enum { PLATFORM_MOTOR_LINK_FRAME_SIZE = 13 };

void platform_motor_link_init(const uint8_t initial_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);
void platform_motor_link_set_transmit(const uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);
bool platform_motor_link_take_received(uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);

#endif
