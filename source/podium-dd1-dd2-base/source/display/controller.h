#ifndef OPENTEC_BASE_DISPLAY_CONTROLLER_H
#define OPENTEC_BASE_DISPLAY_CONTROLLER_H

#include <stdint.h>

typedef enum {
    DISPLAY_BUS_COMMAND,
    DISPLAY_BUS_DATA,
} DisplayBusMode;

typedef void (*DisplayBusWrite)(void *context, DisplayBusMode mode, uint8_t value);

void display_controller_initialize(DisplayBusWrite write, void *context);
void display_controller_begin_frame(DisplayBusWrite write, void *context);

#endif
