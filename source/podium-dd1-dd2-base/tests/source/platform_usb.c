#include <assert.h>
#include <xc.h>

#include "platform/usb.h"

void _USB1Interrupt(void);

static void test_attach_is_complete_and_idempotent(void) {
    platform_usb_init();

    assert(U1CONbits.USBEN == 0);
    assert(U1IE == 0x9f);
    assert(U1EIE == 0x9f);
    assert(IEC5bits.USB1IE == 0);

    platform_usb_attach();

    assert(U1CONbits.USBEN == 1);
    assert(U1IE == 0x9f);
    assert(U1EIE == 0x9f);
    assert(IEC5bits.USB1IE == 1);
    assert(IPC21bits.USB1IP == 4);

    U1EIE = 0x33;
    IPC21bits.USB1IP = 3;
    platform_usb_attach();

    assert(U1EIE == 0x33);
    assert(IPC21bits.USB1IP == 3);
}

static void test_detach_allows_reattachment(void) {
    platform_usb_detach();

    assert(U1CON == 0);
    assert(U1IE == 0);
    assert(IEC5bits.USB1IE == 0);

    platform_usb_attach();

    assert(U1CONbits.USBEN == 1);
    assert(U1CNFG1 == 0);
    assert(U1EIE == 0x9f);
    assert(U1IE == 0x9f);
    assert(IEC5bits.USB1IE == 1);
}

static void test_halts_both_ping_pong_banks(void) {
    platform_usb_init();
    platform_usb_configure_endpoint(1, true, false);
    platform_usb_set_endpoint_halt(0x81, true);
    assert(platform_usb_endpoint_halted(0x81));

    U1STAT = 0x18;
    U1IRbits.TRNIF = 1;
    _USB1Interrupt();

    assert(platform_usb_endpoint_halted(0x81));
}

int main(void) {
    test_attach_is_complete_and_idempotent();
    test_detach_allows_reattachment();
    test_halts_both_ping_pong_banks();
    return 0;
}
