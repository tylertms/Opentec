#include "platform/usb.h"

#include <libpic30.h>
#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

#include "usb/buffer_descriptor.h"

enum {
    USB_ENDPOINT_COUNT = 5,
    USB_BANK_COUNT = 2,
    USB_DIRECTION_COUNT = 2,
    USB_DESCRIPTOR_COUNT = USB_ENDPOINT_COUNT * USB_BANK_COUNT * USB_DIRECTION_COUNT,
    USB_EVENT_CAPACITY = 8,
    USB_EVENT_MASK = USB_EVENT_CAPACITY - 1,
    USB_INTERRUPT_PRIORITY = 4,
    USB_PACKET_ID_SETUP = 0x0d,
    USB_TRANSACTION_ODD_BANK = 0x04,
    USB_TRANSACTION_INPUT = 0x08,
    USB_TRANSACTION_ENDPOINT_MASK = 0xf0,
    USB_ENDPOINT_STALL = 0x02,
    USB_ENDPOINT_HANDSHAKE = 0x01,
    USB_ENDPOINT_TRANSMIT = 0x04,
    USB_ENDPOINT_RECEIVE = 0x08,
    USB_ENDPOINT_CONTROL_DISABLE = 0x10,
    USB_ENDPOINT_CONTROL = USB_ENDPOINT_HANDSHAKE | USB_ENDPOINT_TRANSMIT | USB_ENDPOINT_RECEIVE,
    USB_ENDPOINT_DIRECTION_IN = 0x80,
    USB_ENDPOINT_NUMBER_MASK = 0x0f,
    USB_ENDPOINT_ADDRESS_RESERVED_MASK = 0x70,
};

static const uint32_t USB_RESTART_DELAY_CYCLES = 0x9000UL * 0x0c81UL * 2UL;
static const uint32_t USB_RESUME_PREPARE_DELAY_CYCLES = 0x0e10UL;
static const uint32_t USB_RESUME_SIGNAL_DELAY_CYCLES = 0x09c4UL;

static volatile UsbBufferDescriptor descriptors[USB_DESCRIPTOR_COUNT] __attribute__((aligned(512)));
static volatile uint8_t buffers[USB_ENDPOINT_COUNT][USB_DIRECTION_COUNT][USB_BANK_COUNT]
                               [PLATFORM_USB_PACKET_SIZE];
static volatile PlatformUsbEvent events[USB_EVENT_CAPACITY];
static volatile uint8_t event_head;
static volatile uint8_t event_tail;
static volatile uint8_t next_bank[USB_ENDPOINT_COUNT][USB_DIRECTION_COUNT];

/**
 * @brief Selects one USB buffer descriptor.
 *
 * Maps an endpoint, direction, and ping-pong bank to its controller descriptor.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] input True for device-to-host transfers.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @return Selected descriptor.
 */
static volatile UsbBufferDescriptor *descriptor(uint8_t endpoint, bool input, bool odd_bank) {
    return &descriptors[usb_buffer_descriptor_index(endpoint, input, odd_bank)];
}

/**
 * @brief Selects one USB transfer buffer address.
 *
 * Maps an endpoint, direction, and ping-pong bank to its 64-byte packet buffer.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] input True for device-to-host transfers.
 * @param[in] odd_bank True for the odd ping-pong bank.
 * @return Controller-visible packet buffer address.
 */
static uint16_t buffer_address(uint8_t endpoint, bool input, bool odd_bank) {
    return (uint16_t)&buffers[endpoint][input ? 1 : 0][odd_bank ? 1 : 0][0];
}

/**
 * @brief Clears every USB buffer descriptor.
 *
 * Resets all 20 endpoint, direction, and ping-pong descriptor combinations.
 *
 */
static void clear_descriptors(void) {
    for (uint8_t index = 0; index < USB_DESCRIPTOR_COUNT; index++) {
        usb_buffer_descriptor_clear(&descriptors[index]);
    }
}

/**
 * @brief Arms one endpoint-zero setup bank.
 *
 * Prepares the selected host-to-device bank to accept a setup packet of up to 64 bytes.
 *
 * @param[in] odd_bank True for the odd ping-pong bank.
 */
static void arm_setup_bank(bool odd_bank) {
    usb_buffer_descriptor_arm_setup(descriptor(0, false, odd_bank),
                                    buffer_address(0, false, odd_bank), PLATFORM_USB_PACKET_SIZE);
}

/**
 * @brief Queues one USB controller event.
 *
 * Copies the completed packet into the eight-entry interrupt-to-main event ring and drops the
 * event when the ring is full.
 *
 * @param[in] type Controller event type.
 * @param[in] endpoint Endpoint associated with the event.
 * @param[in] data Completed packet bytes, or null for events without a payload.
 * @param[in] length Number of packet bytes to copy.
 */
static void push_event(PlatformUsbEventType type, uint8_t endpoint, const volatile uint8_t *data,
                       uint8_t length) {
    uint8_t next = (event_head + 1) & USB_EVENT_MASK;
    if (next == event_tail) {
        return;
    }

    volatile PlatformUsbEvent *event = &events[event_head];
    event->type = type;
    event->endpoint = endpoint;
    event->length = length;
    for (uint8_t index = 0; index < length; index++) {
        event->data[index] = data[index];
    }
    event_head = next;
}

/**
 * @brief Restores the USB controller to its default device state.
 *
 * Resets ping-pong selection, address zero, endpoint controls, queued events, descriptors, and
 * both endpoint-zero setup banks before allowing token processing.
 *
 */
static void reset_controller(void) {
    U1CONbits.PPBRST = 1;
    U1ADDR = 0;
    for (uint8_t endpoint = 0; endpoint < USB_ENDPOINT_COUNT; endpoint++) {
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control = endpoint == 0 ? USB_ENDPOINT_CONTROL : 0;
    }
    event_head = 0;
    event_tail = 0;
    clear_descriptors();
    for (uint8_t endpoint = 0; endpoint < USB_ENDPOINT_COUNT; endpoint++) {
        next_bank[endpoint][0] = 0;
        next_bank[endpoint][1] = 0;
    }
    arm_setup_bank(false);
    arm_setup_bank(true);
    U1CONbits.PKTDIS = 0;
    U1CONbits.PPBRST = 0;
}

/**
 * @brief Initializes the USB device controller.
 *
 * Configures the active-high RB1 connection input, enables and powers the controller, selects all
 * supported device interrupt sources, installs the aligned descriptor table, resets endpoint
 * state, and selects interrupt priority four.
 *
 */
void platform_usb_init(void) {
    IEC5bits.USB1IE = 0;
    ANSELBbits.ANSB1 = 0;
    TRISBbits.TRISB1 = 1;
    PMD4bits.USB1MD = 0;
    U1CON = 0;
    U1IE = 0;
    U1EIR = 0xff;
    U1IR = 0xff;
    for (uint8_t endpoint = 0; endpoint < USB_ENDPOINT_COUNT; endpoint++) {
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control = 0;
    }
    U1CNFG1 = 0;
    U1EIE = 0x9f;
    U1PWRCbits.USBPWR = 1;
    U1BDTP1 = (uint16_t)((uint16_t)&descriptors[0] >> 8);
    reset_controller();
    IPC21bits.USB1IP = USB_INTERRUPT_PRIORITY;
    IFS5bits.USB1IF = 0;
}

/**
 * @brief Reads the USB VBUS connection input.
 *
 * Reports the active-high connection sense sampled on PORTB bit 1.
 *
 * @return True while USB VBUS is present.
 */
bool platform_usb_connected(void) { return PORTBbits.RB1 != 0; }

/**
 * @brief Attaches the USB device controller to the bus.
 *
 * Clears pending controller and CPU interrupt flags, enables the supported device events and the
 * priority-four interrupt, and enables the USB transceiver.
 *
 */
void platform_usb_attach(void) {
    U1IR = 0xff;
    U1IE = 0x9f;
    IFS5bits.USB1IF = 0;
    IEC5bits.USB1IE = 1;
    U1CONbits.USBEN = 1;
}

/**
 * @brief Detaches the USB device controller from the bus.
 *
 * Disables the CPU interrupt, USB transceiver, and controller event sources.
 *
 */
void platform_usb_detach(void) {
    IEC5bits.USB1IE = 0;
    U1CONbits.USBEN = 0;
    U1IE = 0;
}

/**
 * @brief Restarts the USB device controller after its reattachment delay.
 *
 * Disables the controller, discards queued transfers, waits for 0x9000 groups of 0x0c81
 * two-cycle delay operations, resets both endpoint banks, and reconnects to the bus.
 *
 */
void platform_usb_restart(void) {
    platform_usb_detach();
    event_head = 0;
    event_tail = 0;
    __delay32(USB_RESTART_DELAY_CYCLES);
    reset_controller();
    platform_usb_attach();
}

/**
 * @brief Signals USB resume after a suspended Xbox host-capability exchange.
 *
 * Disables the USB interrupt, waits 0x0e10 delay cycles, asserts the controller resume bit for
 * 0x09c4 delay cycles, clears the bit, and re-enables the USB interrupt.
 *
 */
void platform_usb_signal_resume(void) {
    IEC5bits.USB1IE = 0;
    __delay32(USB_RESUME_PREPARE_DELAY_CYCLES);
    U1CONbits.RESUME = 1;
    __delay32(USB_RESUME_SIGNAL_DELAY_CYCLES);
    U1CONbits.RESUME = 0;
    IEC5bits.USB1IE = 1;
}

/**
 * @brief Takes one queued USB controller event.
 *
 * Copies the oldest event while briefly excluding the USB interrupt and preserves the prior
 * interrupt-enable state.
 *
 * @param[out] event Destination for the event and any packet bytes.
 * @return True when an event was available; otherwise false.
 */
bool platform_usb_take_event(PlatformUsbEvent *event) {
    if (event == 0) {
        return false;
    }
    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    bool available = event_tail != event_head;
    if (available) {
        const volatile PlatformUsbEvent *source = &events[event_tail];
        event->type = source->type;
        event->endpoint = source->endpoint;
        event->length = source->length;
        for (uint8_t index = 0; index < source->length; index++) {
            event->data[index] = source->data[index];
        }
        event_tail = (event_tail + 1) & USB_EVENT_MASK;
    }
    IEC5bits.USB1IE = interrupt_enabled;
    return available;
}

/**
 * @brief Arms one USB endpoint transfer.
 *
 * Selects the next available ping-pong bank, copies device-to-host bytes, publishes the descriptor
 * to the controller, advances bank selection, and releases endpoint-zero token processing only
 * after the requested control stage is ready.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] input True for a device-to-host transfer.
 * @param[in] data Device-to-host packet bytes, or null for a host-to-device transfer.
 * @param[in] length Transfer capacity in bytes.
 * @param[in] data_one True to use the DATA1 toggle.
 * @return True when a bank accepted the transfer; otherwise false.
 */
static bool arm(uint8_t endpoint, bool input, const uint8_t *data, uint8_t length, bool data_one) {
    if (endpoint >= USB_ENDPOINT_COUNT || length > PLATFORM_USB_PACKET_SIZE) {
        return false;
    }

    uint8_t direction = input ? 1 : 0;
    if (endpoint != 0 && usb_buffer_descriptor_halted(
                             descriptor(endpoint, input, next_bank[endpoint][direction] != 0))) {
        return false;
    }
    uint8_t preferred = next_bank[endpoint][direction];
    for (uint8_t offset = 0; offset < USB_BANK_COUNT; offset++) {
        uint8_t bank = (preferred + offset) & 1;
        volatile UsbBufferDescriptor *target = descriptor(endpoint, input, bank != 0);
        bool replace_setup = endpoint == 0 && !input && offset == 0;
        if (usb_buffer_descriptor_owned(target) && !replace_setup) {
            continue;
        }
        if (input) {
            volatile uint8_t *destination = &buffers[endpoint][1][bank][0];
            for (uint8_t index = 0; index < length; index++) {
                destination[index] = data[index];
            }
        }
        usb_buffer_descriptor_arm(target, buffer_address(endpoint, input, bank != 0), length,
                                  data_one, false);
        next_bank[endpoint][direction] = bank ^ 1;
        if (endpoint == 0) {
            U1CONbits.PKTDIS = 0;
        }
        return true;
    }
    return false;
}

/**
 * @brief Submits one device-to-host USB transfer.
 *
 * Accepts a null source only for a zero-length packet and arms the next endpoint input bank.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] data Packet bytes, or null for a zero-length packet.
 * @param[in] length Number of packet bytes.
 * @param[in] data_one True to use the DATA1 toggle.
 * @return True when the transfer was armed; otherwise false.
 */
bool platform_usb_send(uint8_t endpoint, const uint8_t *data, uint8_t length, bool data_one) {
    return (data != 0 || length == 0) && arm(endpoint, true, data, length, data_one);
}

/**
 * @brief Submits one host-to-device USB transfer.
 *
 * Arms the next endpoint output bank with the requested capacity and data toggle.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 * @param[in] length Maximum packet length.
 * @param[in] data_one True to use the DATA1 toggle.
 * @return True when the transfer was armed; otherwise false.
 */
bool platform_usb_receive(uint8_t endpoint, uint8_t length, bool data_one) {
    return arm(endpoint, false, 0, length, data_one);
}

/**
 * @brief Prepares endpoint zero for the next setup packet.
 *
 * Arms every available output bank for setup traffic, clears a prior control stall, releases token
 * processing, and preserves the prior interrupt-enable state.
 *
 */
void platform_usb_control_ready(void) {
    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    for (uint8_t bank = 0; bank < USB_BANK_COUNT; bank++) {
        volatile UsbBufferDescriptor *target = descriptor(0, false, bank != 0);
        if (!usb_buffer_descriptor_owned(target)) {
            usb_buffer_descriptor_arm_setup(target, buffer_address(0, false, bank != 0),
                                            PLATFORM_USB_PACKET_SIZE);
        }
    }
    U1EP0bits.EPSTALL = 0;
    U1CONbits.PKTDIS = 0;
    IEC5bits.USB1IE = interrupt_enabled;
}

/**
 * @brief Applies the enumerated USB device address.
 *
 * Writes the low seven address bits after the status stage completes.
 *
 * @param[in] address Host-assigned device address.
 */
void platform_usb_set_address(uint8_t address) { U1ADDR = address & 0x7f; }

/**
 * @brief Configures a non-control USB endpoint.
 *
 * Enables handshake processing and the selected device-to-host and host-to-device transfer
 * directions while disabling control transfers on the endpoint.
 *
 * @param[in] endpoint Endpoint number from 1 through 4.
 * @param[in] input True to enable device-to-host transfers.
 * @param[in] output True to enable host-to-device transfers.
 */
void platform_usb_configure_endpoint(uint8_t endpoint, bool input, bool output) {
    if (endpoint == 0 || endpoint >= USB_ENDPOINT_COUNT) {
        return;
    }
    uint16_t control = USB_ENDPOINT_HANDSHAKE | USB_ENDPOINT_CONTROL_DISABLE;
    if (input) {
        control |= USB_ENDPOINT_TRANSMIT;
    }
    if (output) {
        control |= USB_ENDPOINT_RECEIVE;
    }
    volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
    *endpoint_control = control;
}

/**
 * @brief Unconfigures a non-control USB endpoint.
 *
 * Disables the endpoint, clears both directions and banks, and resets its next-bank selection.
 *
 * @param[in] endpoint Endpoint number from 1 through 4.
 */
void platform_usb_unconfigure_endpoint(uint8_t endpoint) {
    if (endpoint == 0 || endpoint >= USB_ENDPOINT_COUNT) {
        return;
    }
    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
    *endpoint_control = 0;
    for (uint8_t direction = 0; direction < USB_DIRECTION_COUNT; direction++) {
        for (uint8_t bank = 0; bank < USB_BANK_COUNT; bank++) {
            usb_buffer_descriptor_clear(descriptor(endpoint, direction != 0, bank != 0));
        }
        next_bank[endpoint][direction] = 0;
    }
    IEC5bits.USB1IE = interrupt_enabled;
}

/**
 * @brief Stalls one USB endpoint.
 *
 * Keeps endpoint zero able to accept a replacement setup packet, requests a stall handshake, and
 * releases token processing after the stall path is ready.
 *
 * @param[in] endpoint Endpoint number from zero through four.
 */
void platform_usb_stall(uint8_t endpoint) {
    if (endpoint < USB_ENDPOINT_COUNT) {
        if (endpoint == 0) {
            for (uint8_t bank = 0; bank < USB_BANK_COUNT; bank++) {
                volatile UsbBufferDescriptor *target = descriptor(0, false, bank != 0);
                if (!usb_buffer_descriptor_owned(target)) {
                    arm_setup_bank(bank != 0);
                }
            }
        }
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control |= USB_ENDPOINT_STALL;
        if (endpoint == 0) {
            U1CONbits.PKTDIS = 0;
        }
    }
}

/**
 * @brief Reports the halt state of one USB endpoint direction.
 *
 * Selects the current ping-pong descriptor from the endpoint address and reports a halt only while
 * that descriptor is stalled and controller-owned.
 *
 * @param[in] endpoint_address Endpoint number with bit seven set for device-to-host direction.
 * @return True when the selected endpoint direction is halted; otherwise false.
 */
bool platform_usb_endpoint_halted(uint8_t endpoint_address) {
    uint8_t endpoint = endpoint_address & USB_ENDPOINT_NUMBER_MASK;
    if ((endpoint_address & USB_ENDPOINT_ADDRESS_RESERVED_MASK) != 0 ||
        endpoint >= USB_ENDPOINT_COUNT) {
        return false;
    }
    bool input = (endpoint_address & USB_ENDPOINT_DIRECTION_IN) != 0;
    uint8_t direction = input ? 1 : 0;
    return usb_buffer_descriptor_halted(
        descriptor(endpoint, input, next_bank[endpoint][direction] != 0));
}

/**
 * @brief Changes the halt state of one non-control USB endpoint direction.
 *
 * A set operation presents a stall on the selected bank. A clear operation releases both banks,
 * restores DATA0/DATA1 ordering, and clears the endpoint stall latch while preserving the other
 * direction's configuration.
 *
 * @param[in] endpoint_address Endpoint number with bit seven set for device-to-host direction.
 * @param[in] halted True to set the halt; false to clear it.
 */
void platform_usb_set_endpoint_halt(uint8_t endpoint_address, bool halted) {
    uint8_t endpoint = endpoint_address & USB_ENDPOINT_NUMBER_MASK;
    if ((endpoint_address & USB_ENDPOINT_ADDRESS_RESERVED_MASK) != 0 || endpoint == 0 ||
        endpoint >= USB_ENDPOINT_COUNT) {
        return;
    }

    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    bool input = (endpoint_address & USB_ENDPOINT_DIRECTION_IN) != 0;
    uint8_t direction = input ? 1 : 0;
    bool odd_bank = next_bank[endpoint][direction] != 0;
    volatile UsbBufferDescriptor *selected = descriptor(endpoint, input, odd_bank);
    if (halted) {
        usb_buffer_descriptor_set_halt(selected);
    } else {
        usb_buffer_descriptor_clear_halt(selected, descriptor(endpoint, input, !odd_bank));
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control &= (uint16_t)~USB_ENDPOINT_STALL;
    }
    IEC5bits.USB1IE = interrupt_enabled;
}

/**
 * @brief Captures one completed USB transaction.
 *
 * Decodes endpoint, direction, and ping-pong bank from controller status, advances the bank
 * selector, and queues the completed setup, output, or input-completion event. Setup traffic clears
 * a prior control stall but remains token-gated until its next stage is armed.
 *
 */
static void handle_transaction(void) {
    uint8_t status = U1STAT;
    uint8_t endpoint = (status & USB_TRANSACTION_ENDPOINT_MASK) >> 4;
    bool input = (status & USB_TRANSACTION_INPUT) != 0;
    bool odd_bank = (status & USB_TRANSACTION_ODD_BANK) != 0;
    if (endpoint >= USB_ENDPOINT_COUNT) {
        return;
    }
    next_bank[endpoint][input ? 1 : 0] = odd_bank ? 0 : 1;
    volatile UsbBufferDescriptor *completed = descriptor(endpoint, input, odd_bank);
    uint8_t length = (uint8_t)usb_buffer_descriptor_count(completed);

    if (input) {
        push_event(PLATFORM_USB_EVENT_IN_COMPLETE, endpoint, 0, 0);
        return;
    }

    const volatile uint8_t *data = &buffers[endpoint][0][odd_bank ? 1 : 0][0];
    PlatformUsbEventType type = usb_buffer_descriptor_packet_id(completed) == USB_PACKET_ID_SETUP
                                    ? PLATFORM_USB_EVENT_SETUP
                                    : PLATFORM_USB_EVENT_OUT;
    push_event(type, endpoint, data, length);
    if (type == PLATFORM_USB_EVENT_SETUP) {
        U1EP0bits.EPSTALL = 0;
    }
}

/**
 * @brief Services USB device-controller interrupts.
 *
 * Handles reset, suspend, protocol errors, frame, stall, and up to four queued transaction events
 * before clearing the CPU interrupt request.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _USB1Interrupt(void) {
    if (U1IRbits.URSTIF != 0) {
        reset_controller();
        push_event(PLATFORM_USB_EVENT_RESET, 0, 0, 0);
        U1IR = 0x01;
    }
    if (U1IRbits.IDLEIF != 0) {
        push_event(PLATFORM_USB_EVENT_SUSPEND, 0, 0, 0);
        U1IR = 0x10;
    }
    if (U1IRbits.UERRIF != 0) {
        U1EIR = 0xff;
        U1IR = 0x02;
    }
    if (U1IRbits.SOFIF != 0) {
        U1IR = 0x04;
    }
    if (U1IRbits.STALLIF != 0) {
        U1EP0bits.EPSTALL = 0;
        U1IR = 0x80;
    }
    for (uint8_t transaction = 0; transaction < 4 && U1IRbits.TRNIF != 0; transaction++) {
        handle_transaction();
        U1IR = 0x08;
    }
    IFS5bits.USB1IF = 0;
}
