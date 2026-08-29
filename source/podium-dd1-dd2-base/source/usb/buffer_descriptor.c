#include "usb/buffer_descriptor.h"

#include <stdbool.h>
#include <stdint.h>

typedef char UsbBufferDescriptorSizeIsEightBytes
    [sizeof(UsbBufferDescriptor) == USB_BUFFER_DESCRIPTOR_BYTES ? 1 : -1];

/**
 * @brief Calculates one USB buffer descriptor index.
 *
 * Maps the endpoint, transfer direction, and ping-pong bank to the controller's four-descriptor
 * endpoint stride.
 *
 * @param[in] endpoint USB endpoint number.
 * @param[in] input True for a device-to-host descriptor.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @return Descriptor table index.
 */
uint8_t usb_buffer_descriptor_index(uint8_t endpoint, bool input, bool odd_bank) {
    return (uint8_t)((endpoint << 2) | (input ? 2 : 0) | (odd_bank ? 1 : 0));
}

/**
 * @brief Clears one USB buffer descriptor.
 *
 * Removes controller ownership, byte count, and transfer address from the descriptor.
 *
 * @param[out] descriptor Descriptor to clear.
 */
void usb_buffer_descriptor_clear(volatile UsbBufferDescriptor *descriptor) {
    descriptor->status = 0;
    descriptor->count = 0;
    descriptor->address = 0;
    descriptor->address_high = 0;
}

/**
 * @brief Arms one USB setup descriptor.
 *
 * Publishes the transfer address and capacity before giving the controller ownership with setup
 * stall handling enabled.
 *
 * @param[out] descriptor Descriptor to arm.
 * @param[in] address Controller-visible transfer address.
 * @param[in] capacity Maximum accepted byte count.
 */
void usb_buffer_descriptor_arm_setup(volatile UsbBufferDescriptor *descriptor, uint32_t address,
                                     uint16_t capacity) {
    descriptor->address = (uint16_t)address;
    descriptor->address_high = (uint16_t)(address >> 16);
    descriptor->count = capacity & USB_BUFFER_COUNT_MASK;
    descriptor->status = USB_BUFFER_STALL | USB_BUFFER_OWNED_BY_USB;
}

/**
 * @brief Arms one USB data descriptor.
 *
 * Publishes the transfer address, capacity, data toggle, optional stall response, and controller
 * ownership in controller-safe order.
 *
 * @param[out] descriptor Descriptor to arm.
 * @param[in] address Controller-visible transfer address.
 * @param[in] capacity Maximum transfer byte count.
 * @param[in] data_one True to use the DATA1 toggle.
 * @param[in] stall True to request a stall response.
 */
void usb_buffer_descriptor_arm(volatile UsbBufferDescriptor *descriptor, uint32_t address,
                               uint16_t capacity, bool data_one, bool stall) {
    descriptor->address = (uint16_t)address;
    descriptor->address_high = (uint16_t)(address >> 16);
    descriptor->count = capacity & USB_BUFFER_COUNT_MASK;
    descriptor->status = USB_BUFFER_DATA_TOGGLE_ENABLED | (data_one ? USB_BUFFER_DATA_TOGGLE : 0) |
                         (stall ? USB_BUFFER_STALL : 0) | USB_BUFFER_OWNED_BY_USB;
}

/**
 * @brief Tests whether the USB controller owns a descriptor.
 *
 * Reads the controller-ownership flag from the descriptor status field.
 *
 * @param[in] descriptor Descriptor to inspect.
 * @return True while the controller owns the descriptor.
 */
bool usb_buffer_descriptor_owned(const volatile UsbBufferDescriptor *descriptor) {
    return (descriptor->status & USB_BUFFER_OWNED_BY_USB) != 0;
}

/**
 * @brief Reads a USB descriptor byte count.
 *
 * Removes reserved count bits and returns the low ten-bit transfer count.
 *
 * @param[in] descriptor Descriptor to inspect.
 * @return Transfer byte count.
 */
uint16_t usb_buffer_descriptor_count(const volatile UsbBufferDescriptor *descriptor) {
    return descriptor->count & USB_BUFFER_COUNT_MASK;
}

/**
 * @brief Reads a completed USB packet identifier.
 *
 * Extracts the four-bit packet identifier from descriptor status.
 *
 * @param[in] descriptor Completed descriptor to inspect.
 * @return Four-bit packet identifier.
 */
uint8_t usb_buffer_descriptor_packet_id(const volatile UsbBufferDescriptor *descriptor) {
    return (uint8_t)((descriptor->status & USB_BUFFER_PACKET_ID_MASK) >>
                     USB_BUFFER_PACKET_ID_SHIFT);
}
