#ifndef OPENTEC_BASE_PLATFORM_SERIAL_LINK_H
#define OPENTEC_BASE_PLATFORM_SERIAL_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/packet.h"

void platform_serial_link_init(void);
void platform_serial_link_reset(void);
bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]);
bool platform_serial_link_take_received(uint8_t packet[SERIAL_PACKET_SIZE]);

#endif
