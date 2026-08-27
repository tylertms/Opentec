#ifndef OPENTEC_BASE_USB_BUFFER_DESCRIPTOR_H
#define OPENTEC_BASE_USB_BUFFER_DESCRIPTOR_H

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_BUFFER_DESCRIPTOR_BYTES = 8,
    USB_BUFFER_STALL = 0x04,
    USB_BUFFER_DATA_TOGGLE_ENABLED = 0x08,
    USB_BUFFER_DATA_TOGGLE = 0x40,
    USB_BUFFER_OWNED_BY_USB = 0x80,
    USB_BUFFER_PACKET_ID_MASK = 0x3c,
    USB_BUFFER_PACKET_ID_SHIFT = 2,
    USB_BUFFER_COUNT_MASK = 0x03ff,
};

typedef struct {
    uint16_t status;
    uint16_t count;
    uint16_t address;
    uint16_t address_high;
} UsbBufferDescriptor;

uint8_t usb_buffer_descriptor_index(uint8_t endpoint, bool input, bool odd_bank);
void usb_buffer_descriptor_clear(volatile UsbBufferDescriptor *descriptor);
void usb_buffer_descriptor_arm(volatile UsbBufferDescriptor *descriptor, uint32_t address,
                               uint16_t capacity, bool data_one, bool stall);
bool usb_buffer_descriptor_owned(const volatile UsbBufferDescriptor *descriptor);
uint16_t usb_buffer_descriptor_count(const volatile UsbBufferDescriptor *descriptor);
uint8_t usb_buffer_descriptor_packet_id(const volatile UsbBufferDescriptor *descriptor);

#endif
