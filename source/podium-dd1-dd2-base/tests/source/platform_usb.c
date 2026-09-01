#include <assert.h>
#include <xc.h>

#include "platform/usb.h"

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

int main(void) {
    test_attach_is_complete_and_idempotent();
    test_detach_allows_reattachment();
    return 0;
}
