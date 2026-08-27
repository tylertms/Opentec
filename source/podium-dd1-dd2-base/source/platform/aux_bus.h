#ifndef OPENTEC_BASE_PLATFORM_AUX_BUS_H
#define OPENTEC_BASE_PLATFORM_AUX_BUS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PLATFORM_AUX_BUS_IDLE,
    PLATFORM_AUX_BUS_BUSY,
    PLATFORM_AUX_BUS_SUCCEEDED,
    PLATFORM_AUX_BUS_FAILED,
} PlatformAuxBusStatus;

void platform_aux_bus_init(void);
void platform_aux_bus_service(void);
bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length);
bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length);
PlatformAuxBusStatus platform_aux_bus_status(void);
void platform_aux_bus_clear(void);

#endif
