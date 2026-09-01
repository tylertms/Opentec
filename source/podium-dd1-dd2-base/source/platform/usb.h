#ifndef OPENTEC_BASE_PLATFORM_USB_H
#define OPENTEC_BASE_PLATFORM_USB_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Maximum USB packet payload supported by the platform buffers.
 */
enum {
    PLATFORM_USB_PACKET_SIZE = 64 /**< Maximum USB packet payload in bytes. */
};

/**
 * @brief Type of event reported by the USB device controller.
 */
typedef enum {
    PLATFORM_USB_EVENT_RESET,       /**< A USB bus reset was detected. */
    PLATFORM_USB_EVENT_SETUP,       /**< A control setup packet was received. */
    PLATFORM_USB_EVENT_OUT,         /**< A host-to-device data packet was received. */
    PLATFORM_USB_EVENT_IN_COMPLETE, /**< A device-to-host transfer completed. */
    PLATFORM_USB_EVENT_SUSPEND,     /**< The USB bus entered suspend. */
} PlatformUsbEventType;

/**
 * @brief USB controller event and optional packet payload.
 */
typedef struct {
    PlatformUsbEventType type;              /**< Event category. */
    uint8_t endpoint;                       /**< Endpoint number associated with the event. */
    uint8_t length;                         /**< Number of valid bytes in data. */
    uint8_t data[PLATFORM_USB_PACKET_SIZE]; /**< Packet bytes copied from the controller buffer. */
} PlatformUsbEvent;

/**
 * @brief Initializes the USB device controller.
 *
 * Configures the controller, descriptor table, endpoint-zero setup buffers, and interrupt routing.
 */
void platform_usb_init(void);

/**
 * @brief Reports whether USB VBUS is present.
 *
 * Reads the board's active-high USB connection-sense input.
 *
 * @return True when VBUS is detected; otherwise false.
 */
bool platform_usb_connected(void);

/**
 * @brief Attaches the USB device to the bus.
 *
 * Leaves an active attachment unchanged. From the detached state, restores controller event and
 * interrupt configuration, enables the USB transceiver, and records the attached state.
 */
void platform_usb_attach(void);

/**
 * @brief Detaches the USB device from the bus.
 *
 * Disables the USB transceiver and device-controller interrupt sources and records the detached
 * state.
 */
void platform_usb_detach(void);

/**
 * @brief Restarts the USB device controller.
 *
 * Detaches the controller, waits the required reattachment interval, resets its endpoint state,
 * and attaches it again.
 */
void platform_usb_restart(void);

/**
 * @brief Signals USB resume to the host.
 *
 * Generates the controller resume pulse while USB interrupts are disabled.
 */
void platform_usb_signal_resume(void);

/**
 * @brief Takes the oldest queued USB controller event.
 *
 * Copies and consumes one event produced by the interrupt handler. A null destination is rejected.
 *
 * @param[out] event Destination for the event and its packet payload.
 * @return True when an event was available; otherwise false.
 */
bool platform_usb_take_event(PlatformUsbEvent *event);

/**
 * @brief Queues a device-to-host USB transfer.
 *
 * Copies the packet into an available endpoint bank and applies the requested data toggle.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] data Packet source, or null when length is zero.
 * @param[in] length Number of packet bytes, up to PLATFORM_USB_PACKET_SIZE.
 * @param[in] data_one True to use the DATA1 toggle; false to use DATA0.
 * @return True when the transfer was queued; otherwise false.
 */
bool platform_usb_send(uint8_t endpoint, const uint8_t *data, uint8_t length, bool data_one);

/**
 * @brief Queues a host-to-device USB transfer.
 *
 * Arms an available endpoint bank to receive up to the requested packet length.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] length Maximum packet length, up to PLATFORM_USB_PACKET_SIZE.
 * @param[in] data_one True to use the DATA1 toggle; false to use DATA0.
 * @return True when the receive bank was queued; otherwise false.
 */
bool platform_usb_receive(uint8_t endpoint, uint8_t length, bool data_one);

/**
 * @brief Prepares endpoint zero for the next control setup packet.
 *
 * Arms available setup banks, clears a control stall, and releases controller token processing.
 */
void platform_usb_control_ready(void);

/**
 * @brief Sets the USB device address.
 *
 * Stores the low seven bits of the host-assigned address in the controller.
 *
 * @param[in] address Host-assigned device address.
 */
void platform_usb_set_address(uint8_t address);

/**
 * @brief Configures a non-control USB endpoint.
 *
 * Enables the selected device-to-host and host-to-device directions.
 *
 * @param[in] endpoint Endpoint number from one through four.
 * @param[in] input True to enable device-to-host transfers.
 * @param[in] output True to enable host-to-device transfers.
 */
void platform_usb_configure_endpoint(uint8_t endpoint, bool input, bool output);

/**
 * @brief Unconfigures a non-control USB endpoint.
 *
 * Disables both transfer directions, clears endpoint banks, and resets their bank selection.
 *
 * @param[in] endpoint Endpoint number from one through four.
 */
void platform_usb_unconfigure_endpoint(uint8_t endpoint);

/**
 * @brief Stalls a USB endpoint.
 *
 * Requests a stall handshake for the endpoint and keeps endpoint zero ready for a replacement
 * setup packet.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 */
void platform_usb_stall(uint8_t endpoint);

/**
 * @brief Reports whether a USB endpoint direction is halted.
 *
 * Interprets the endpoint address direction bit and inspects the selected controller bank.
 *
 * @param[in] endpoint_address Endpoint number with bit seven set for device-to-host direction.
 * @return True when the endpoint direction is halted; otherwise false.
 */
bool platform_usb_endpoint_halted(uint8_t endpoint_address);

/**
 * @brief Changes the halt state of a non-control USB endpoint direction.
 *
 * Sets or clears both ping-pong banks for the selected endpoint direction and preserves the other
 * direction.
 *
 * @param[in] endpoint_address Endpoint number with bit seven set for device-to-host direction.
 * @param[in] halted True to set the halt; false to clear it.
 */
void platform_usb_set_endpoint_halt(uint8_t endpoint_address, bool halted);

#endif
