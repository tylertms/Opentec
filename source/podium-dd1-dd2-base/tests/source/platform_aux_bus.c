#include <assert.h>
#include <stdint.h>
#include <xc.h>

#include "platform/aux_bus.h"

static uint32_t now_ms;

uint32_t platform_time_ms(void) { return now_ms; }

void _MI2C2Interrupt(void);
bool platform_aux_bus_reset_finish_active(void);

int main(void) {
    platform_aux_bus_init();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);
    assert(I2C2CONbits.PEN == 1);
    assert(platform_aux_bus_reset_finish_active());

    I2C2CONbits.PEN = 0;
    _MI2C2Interrupt();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);

    uint8_t response;
    assert(platform_aux_bus_start_read(0x78, 0, &response, 1));
    now_ms = 2;
    platform_aux_bus_service();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_FAILED);
    assert(platform_aux_bus_reset_finish_active());

    I2C2CONbits.PEN = 0;
    _MI2C2Interrupt();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_FAILED);
    return 0;
}
