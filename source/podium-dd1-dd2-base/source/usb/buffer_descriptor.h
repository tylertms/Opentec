#ifndef OPENTEC_BASE_USB_BUFFER_DESCRIPTOR_H
#define OPENTEC_BASE_USB_BUFFER_DESCRIPTOR_H

#include <stdbool.h>
#include <stdint.h>

/** @brief USB buffer-descriptor field sizes and status masks. */
enum {
    USB_BUFFER_DESCRIPTOR_BYTES = 8, /**< Size of one controller buffer descriptor in bytes. */
    USB_BUFFER_STALL = 0x04,         /**< Status bit that requests a stall handshake. */
    USB_BUFFER_DATA_TOGGLE_ENABLED = 0x08, /**< Status bit that enables the data-toggle field. */
    USB_BUFFER_DATA_TOGGLE = 0x40, /**< Status bit selecting DATA1 when data toggling is enabled. */
    USB_BUFFER_OWNED_BY_USB = 0x80, /**< Status bit transferring ownership to the USB controller. */
    USB_BUFFER_PACKET_ID_MASK = 0x3c, /**< Mask covering the completed packet identifier. */
    USB_BUFFER_PACKET_ID_SHIFT = 2,   /**< Right shift needed to read the packet identifier. */
    USB_BUFFER_COUNT_MASK = 0x03ff,   /**< Mask covering the ten-bit transfer count. */
};

/** @brief Controller-facing address, count, and status fields for one USB transfer buffer. */
typedef struct {
    uint16_t status;       /**< Controller status and ownership flags. */
    uint16_t count;        /**< Transfer byte count field. */
    uint16_t address;      /**< Low sixteen bits of the controller-visible transfer address. */
    uint16_t address_high; /**< High sixteen bits of the controller-visible transfer address. */
} UsbBufferDescriptor;

/**
 * @brief Calculates one USB buffer descriptor index.
 *
 * Maps an endpoint number, transfer direction, and ping-pong bank to the controller descriptor
 * table.
 *
 * @param[in] endpoint USB endpoint number.
 * @param[in] input True for a device-to-host descriptor.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @return Descriptor table index.
 */
uint8_t usb_buffer_descriptor_index(uint8_t endpoint, bool input, bool odd_bank);

/**
 * @brief Clears one USB buffer descriptor.
 *
 * Removes controller ownership, byte count, and transfer address from the descriptor.
 *
 * @param[out] descriptor Descriptor to clear.
 */
void usb_buffer_descriptor_clear(volatile UsbBufferDescriptor *descriptor);

/**
 * @brief Arms one USB setup descriptor.
 *
 * Publishes the transfer address and capacity before giving the controller ownership with stall
 * handling enabled.
 *
 * @param[out] descriptor Descriptor to arm.
 * @param[in] address Controller-visible transfer address.
 * @param[in] capacity Maximum accepted byte count.
 */
void usb_buffer_descriptor_arm_setup(volatile UsbBufferDescriptor *descriptor, uint32_t address,
                                     uint16_t capacity);

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
                               uint16_t capacity, bool data_one, bool stall);

/**
 * @brief Tests whether the USB controller owns a descriptor.
 *
 * Reads the controller-ownership flag from the descriptor status field.
 *
 * @param[in] descriptor Descriptor to inspect.
 * @return True while the controller owns the descriptor; otherwise false.
 */
bool usb_buffer_descriptor_owned(const volatile UsbBufferDescriptor *descriptor);

/**
 * @brief Reads a USB descriptor byte count.
 *
 * Removes reserved count bits and returns the low ten-bit transfer count.
 *
 * @param[in] descriptor Descriptor to inspect.
 * @return Transfer byte count.
 */
uint16_t usb_buffer_descriptor_count(const volatile UsbBufferDescriptor *descriptor);

/**
 * @brief Reads a completed USB packet identifier.
 *
 * Extracts the four-bit packet identifier from descriptor status.
 *
 * @param[in] descriptor Completed descriptor to inspect.
 * @return Four-bit packet identifier.
 */
uint8_t usb_buffer_descriptor_packet_id(const volatile UsbBufferDescriptor *descriptor);

/**
 * @brief Reports whether an endpoint descriptor presents a halt handshake.
 *
 * Requires both the stall flag and controller ownership on the selected ping-pong descriptor.
 *
 * @param[in] descriptor Selected endpoint descriptor.
 * @return True when the descriptor currently presents a halt; otherwise false.
 */
bool usb_buffer_descriptor_halted(const volatile UsbBufferDescriptor *descriptor);

/**
 * @brief Presents a halt handshake on an endpoint descriptor.
 *
 * Preserves the transfer fields while setting the stall flag and giving the descriptor to the
 * controller.
 *
 * @param[in,out] descriptor Selected endpoint descriptor.
 */
void usb_buffer_descriptor_set_halt(volatile UsbBufferDescriptor *descriptor);

/**
 * @brief Releases a halted ping-pong descriptor pair.
 *
 * Removes ownership, stall, and DATA1 from the selected bank and prepares the alternate bank for
 * DATA1 so the next transfer sequence restarts at DATA0.
 *
 * @param[in,out] selected Currently selected endpoint descriptor.
 * @param[in,out] alternate Other ping-pong descriptor for the same endpoint direction.
 */
void usb_buffer_descriptor_clear_halt(volatile UsbBufferDescriptor *selected,
                                      volatile UsbBufferDescriptor *alternate);

#endif
