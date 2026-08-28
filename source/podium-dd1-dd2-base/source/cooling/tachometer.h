#ifndef OPENTEC_BASE_COOLING_TACHOMETER_H
#define OPENTEC_BASE_COOLING_TACHOMETER_H

#include <stdint.h>

uint16_t fan_tachometer_rpm(uint32_t previous_capture, uint32_t current_capture);

#endif
