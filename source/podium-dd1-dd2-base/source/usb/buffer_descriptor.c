#include "usb/buffer_descriptor.h"

#include <stdbool.h>
#include <stdint.h>

typedef char UsbBufferDescriptorSizeIsEightBytes
    [sizeof(UsbBufferDescriptor) == USB_BUFFER_DESCRIPTOR_BYTES ? 1 : -1];

uint8_t usb_buffer_descriptor_index(uint8_t endpoint, bool input, bool odd_bank) {
    return (uint8_t)((endpoint << 2) | (input ? 2 : 0) | (odd_bank ? 1 : 0));
}

void usb_buffer_descriptor_clear(volatile UsbBufferDescriptor *descriptor) {
    descriptor->status = 0;
    descriptor->count = 0;
    descriptor->address = 0;
    descriptor->address_high = 0;
}

void usb_buffer_descriptor_arm_setup(volatile UsbBufferDescriptor *descriptor, uint32_t address,
                                     uint16_t capacity) {
    descriptor->address = (uint16_t)address;
    descriptor->address_high = (uint16_t)(address >> 16);
    descriptor->count = capacity & USB_BUFFER_COUNT_MASK;
    descriptor->status = USB_BUFFER_STALL | USB_BUFFER_OWNED_BY_USB;
}

void usb_buffer_descriptor_arm(volatile UsbBufferDescriptor *descriptor, uint32_t address,
                               uint16_t capacity, bool data_one, bool stall) {
    descriptor->address = (uint16_t)address;
    descriptor->address_high = (uint16_t)(address >> 16);
    descriptor->count = capacity & USB_BUFFER_COUNT_MASK;
    descriptor->status = USB_BUFFER_DATA_TOGGLE_ENABLED | (data_one ? USB_BUFFER_DATA_TOGGLE : 0) |
                         (stall ? USB_BUFFER_STALL : 0) | USB_BUFFER_OWNED_BY_USB;
}

bool usb_buffer_descriptor_owned(const volatile UsbBufferDescriptor *descriptor) {
    return (descriptor->status & USB_BUFFER_OWNED_BY_USB) != 0;
}

uint16_t usb_buffer_descriptor_count(const volatile UsbBufferDescriptor *descriptor) {
    return descriptor->count & USB_BUFFER_COUNT_MASK;
}

uint8_t usb_buffer_descriptor_packet_id(const volatile UsbBufferDescriptor *descriptor) {
    return (uint8_t)((descriptor->status & USB_BUFFER_PACKET_ID_MASK) >>
                     USB_BUFFER_PACKET_ID_SHIFT);
}
