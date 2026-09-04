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
 * Configures the controller, descriptor table, endpoint-zero bank-zero setup descriptor, and
 * interrupt routing.
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
 * Copies the packet into the selected endpoint bank and applies the requested data toggle. The
 * endpoint-zero bank remains selected until its completion event advances the control topology.
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
 * Arms the selected endpoint bank to receive up to the requested packet length. The endpoint-zero
 * bank remains selected until its completion event advances the control topology.
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
 * Arms available setup banks after control activity, clears a control stall, and releases
 * controller token processing. Reset initialization publishes only bank-zero setup ownership.
 */
void platform_usb_control_ready(void);

/**
 * @brief Resets endpoint-zero ping-pong state for a new setup transaction.
 *
 * Releases endpoint-zero ownership for the aborted transfer while preserving the controller's
 * current bank and data-toggle topology. The next control-stage arm publishes the descriptors
 * required for that stage.
 */
void platform_usb_control_reset(void);

/**
 * @brief Arms one endpoint-zero control status stage.
 *
 * Arms a DATA1 zero-length input status packet when input is true. When input is false, arms the
 * endpoint-zero output status bank and its setup-bank guard while preserving the current
 * ping-pong selection.
 *
 * @param[in] input True for a device-to-host status stage; false for a host-to-device status
 * stage.
 * @return True when the status stage was armed; otherwise false.
 */
bool platform_usb_control_arm_status(bool input);

/**
 * @brief Resets non-control endpoint state after a configuration change.
 *
 * Disables non-control endpoints, clears every descriptor, resets the controller ping-pong state,
 * and leaves endpoint zero ready for the next control status completion.
 */
void platform_usb_reset_endpoint_state(void);

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

#ifdef OPENTEC_SIMULATOR_TEST
/**
 * @brief Reads one simulator-visible USB descriptor status field.
 *
 * Exposes descriptor ownership and transfer-mode state to platform regression tests without adding
 * a production API.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] input True for a device-to-host descriptor.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @return Descriptor status field.
 */
uint16_t platform_usb_test_descriptor_status(uint8_t endpoint, bool input, bool odd_bank);

/**
 * @brief Reads one simulator-visible USB descriptor count field.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] input True for a device-to-host descriptor.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @return Descriptor count field.
 */
uint16_t platform_usb_test_descriptor_count(uint8_t endpoint, bool input, bool odd_bank);

/**
 * @brief Sets simulator-visible USB descriptor status and count fields.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] input True for a device-to-host descriptor.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @param[in] status Descriptor status field.
 * @param[in] count Descriptor count field.
 */
void platform_usb_test_set_descriptor(uint8_t endpoint, bool input, bool odd_bank, uint16_t status,
                                      uint16_t count);

/**
 * @brief Services a synthetic transaction-complete source for platform tests.
 *
 * Injects a decoded U1STAT value while retaining the production U1IE transaction-source gate.
 *
 * @param[in] status Synthetic U1STAT endpoint, direction, and bank value.
 */
void platform_usb_test_service_transaction(uint8_t status);

/**
 * @brief Services a synthetic start-of-frame source for platform tests.
 *
 * Retains the production U1IE start-of-frame-source gate.
 */
void platform_usb_test_service_sof(void);

#endif

#endif
