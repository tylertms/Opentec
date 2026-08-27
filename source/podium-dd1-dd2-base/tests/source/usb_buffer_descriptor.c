#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb/buffer_descriptor.h"

static void test_bdt_index(void) {
    assert(usb_buffer_descriptor_index(0, false, false) == 0);
    assert(usb_buffer_descriptor_index(0, false, true) == 1);
    assert(usb_buffer_descriptor_index(0, true, false) == 2);
    assert(usb_buffer_descriptor_index(0, true, true) == 3);
    assert(usb_buffer_descriptor_index(3, true, true) == 15);
}

static void test_arm(void) {
    UsbBufferDescriptor descriptor;
    usb_buffer_descriptor_clear(&descriptor);

    usb_buffer_descriptor_arm(&descriptor, 0x12345678, 64, true, false);

    assert(descriptor.address == 0x5678);
    assert(descriptor.address_high == 0x1234);
    assert(usb_buffer_descriptor_count(&descriptor) == 64);
    assert(descriptor.status ==
           (USB_BUFFER_DATA_TOGGLE_ENABLED | USB_BUFFER_DATA_TOGGLE | USB_BUFFER_OWNED_BY_USB));
    assert(usb_buffer_descriptor_owned(&descriptor));
}

static void test_capacity_and_packet_fields_are_masked(void) {
    UsbBufferDescriptor descriptor;
    usb_buffer_descriptor_arm(&descriptor, 0, UINT16_MAX, false, true);
    assert(usb_buffer_descriptor_count(&descriptor) == USB_BUFFER_COUNT_MASK);
    assert((descriptor.status & USB_BUFFER_STALL) != 0);

    descriptor.status = 0x34;
    assert(usb_buffer_descriptor_packet_id(&descriptor) == 13);
}

static void test_setup_arm(void) {
    UsbBufferDescriptor descriptor;
    usb_buffer_descriptor_arm_setup(&descriptor, 0x1234, 64);
    assert(descriptor.status == (USB_BUFFER_STALL | USB_BUFFER_OWNED_BY_USB));
    assert(descriptor.count == 64);
    assert(descriptor.address == 0x1234);
}

int main(void) {
    test_bdt_index();
    test_arm();
    test_capacity_and_packet_fields_are_masked();
    test_setup_arm();
    return 0;
}
