#include <assert.h>
#include <stdint.h>
#include <xc.h>

#include "platform/aux_bus.h"
#include "platform/time.h"

void _MI2C2Interrupt(void);
void _T1Interrupt(void);
bool platform_aux_bus_reset_finish_active(void);

static void consume_reset_stop(void) {
    I2C2CONbits.PEN = 0;
    _MI2C2Interrupt();
}

static void expire_transfer_with_timer(void) {
    _T1Interrupt();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_BUSY);
    _T1Interrupt();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_FAILED);
    assert(platform_aux_bus_reset_finish_active());
    consume_reset_stop();
}

static void test_initialization(void) {
    platform_time_init();
    PORTFbits.RF4 = 0;
    platform_aux_bus_init();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);
    assert(I2C2CONbits.PEN == 1);
    assert(platform_aux_bus_reset_finish_active());
    assert(TRISFbits.TRISF4 == 1);
    assert(TRISFbits.TRISF5 == 1);
    assert(LATFbits.LATF5 == 0);

    consume_reset_stop();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);
}

static void test_foreground_service_does_not_consume_timeout(void) {
    uint8_t response;
    assert(platform_aux_bus_start_read(0x78, 0, &response, sizeof(response)));
    platform_aux_bus_service();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_BUSY);
    IEC3bits.MI2C2IE = 0;
    expire_transfer_with_timer();
}

static void test_retries_are_bounded(void) {
    uint8_t response;
    platform_aux_bus_clear();
    assert(platform_aux_bus_start_read(0x78, 0, &response, sizeof(response)));
    while (platform_aux_bus_status() == PLATFORM_AUX_BUS_BUSY) {
    }
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_FAILED);
    assert(platform_aux_bus_retry_count() == 4);
    assert(!platform_aux_bus_reset_finish_active());
    platform_aux_bus_clear();
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);
}

int main(void) {
    test_initialization();
    test_foreground_service_does_not_consume_timeout();
    test_retries_are_bounded();

    return 0;
}
