#include <assert.h>
#include <stdbool.h>
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

static void test_init_arms_only_ep0_setup_bank_zero(void) {
    platform_usb_init();

    assert(platform_usb_test_descriptor_status(0, false, false) == 0x0084);
    assert(platform_usb_test_descriptor_count(0, false, false) == PLATFORM_USB_PACKET_SIZE);
    assert(platform_usb_test_descriptor_status(0, false, true) == 0);
    assert(platform_usb_test_descriptor_count(0, false, true) == 0);
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

static void test_interrupt_sources_obey_enable_mask(void) {
    platform_usb_init();
    platform_usb_configure_endpoint(1, false, true);
    assert(platform_usb_receive(1, 64, false));

    U1IE &= (uint16_t)~0x08;
    platform_usb_test_service_transaction(0x10);

    PlatformUsbEvent event;
    assert(!platform_usb_take_event(&event));
    U1IE |= 0x08;
    platform_usb_test_service_transaction(0x10);
    assert(platform_usb_take_event(&event));
    assert(event.type == PLATFORM_USB_EVENT_OUT && event.endpoint == 1 && event.length == 64);
}

static void test_sof_recovers_stuck_descriptor_after_45_frames(void) {
    platform_usb_init();
    assert(platform_usb_receive(0, 64, true));

    platform_usb_test_service_transaction(0x00);
    PlatformUsbEvent event;
    assert(platform_usb_take_event(&event));

    platform_usb_test_set_descriptor(0, false, false, 0x008c, 0xc040);
    platform_usb_test_set_descriptor(0, false, true, 0x0084, 0xc041);
    U1IE &= (uint16_t)~0x04;
    for (uint8_t frame = 0; frame < 45; frame++) {
        platform_usb_test_service_sof();
    }
    assert(platform_usb_test_descriptor_status(0, false, false) == 0x008c);
    assert(platform_usb_test_descriptor_count(0, false, false) == 0xc040);
    assert(platform_usb_test_descriptor_status(0, false, true) == 0x0084);
    assert(platform_usb_test_descriptor_count(0, false, true) == 0xc041);

    U1IE |= 0x04;
    for (uint8_t frame = 0; frame < 44; frame++) {
        platform_usb_test_service_sof();
    }
    assert(platform_usb_test_descriptor_status(0, false, false) == 0x008c);
    assert(platform_usb_test_descriptor_count(0, false, false) == 0xc040);
    assert(platform_usb_test_descriptor_status(0, false, true) == 0x0084);
    assert(platform_usb_test_descriptor_count(0, false, true) == 0xc041);

    platform_usb_test_service_sof();
    assert(platform_usb_test_descriptor_status(0, false, false) == 0x00c8);
    assert(platform_usb_test_descriptor_count(0, false, false) == 0x0040);
    assert(platform_usb_test_descriptor_status(0, false, true) == 0x0084);
    assert(platform_usb_test_descriptor_count(0, false, true) == 0x0041);
}

int main(void) {
    test_attach_is_complete_and_idempotent();
    test_detach_allows_reattachment();
    test_init_arms_only_ep0_setup_bank_zero();
    test_halts_both_ping_pong_banks();
    test_interrupt_sources_obey_enable_mask();
    test_sof_recovers_stuck_descriptor_after_45_frames();
    return 0;
}
