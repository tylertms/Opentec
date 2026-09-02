#include "platform/usb.h"

#include <libpic30.h>
#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

#include "usb/buffer_descriptor.h"

/**
 * @brief USB controller dimensions, status masks, and control values.
 */
enum {
    USB_ENDPOINT_COUNT = 5,  /**< Number of endpoint numbers supported by the controller. */
    USB_BANK_COUNT = 2,      /**< Number of ping-pong banks per endpoint direction. */
    USB_DIRECTION_COUNT = 2, /**< Number of endpoint directions. */
    USB_DESCRIPTOR_COUNT =
        USB_ENDPOINT_COUNT * USB_BANK_COUNT * USB_DIRECTION_COUNT, /**< Total descriptor entries. */
    USB_EVENT_CAPACITY = 24,
    USB_INTERRUPT_PRIORITY = 4,           /**< USB interrupt priority. */
    USB_PACKET_ID_SETUP = 0x0d,           /**< USB packet identifier for a setup transaction. */
    USB_TRANSACTION_ODD_BANK = 0x04,      /**< U1STAT mask for the odd ping-pong bank. */
    USB_TRANSACTION_INPUT = 0x08,         /**< U1STAT mask for a device-to-host transaction. */
    USB_INTERRUPT_RESET = 0x01,           /**< U1IR and U1IE bus-reset flag. */
    USB_INTERRUPT_ERROR = 0x02,           /**< U1IR and U1IE transaction-error flag. */
    USB_INTERRUPT_SOF = 0x04,             /**< U1IR and U1IE start-of-frame flag. */
    USB_INTERRUPT_TRANSACTION = 0x08,     /**< U1IR and U1IE transaction-complete flag. */
    USB_INTERRUPT_IDLE = 0x10,            /**< U1IR and U1IE idle flag. */
    USB_INTERRUPT_STALL = 0x80,           /**< U1IR and U1IE stall flag. */
    USB_SOF_RECOVERY_FRAME_COUNT = 0x2d,  /**< SOF frames allowed before descriptor recovery. */
    USB_TRANSACTION_ENDPOINT_MASK = 0xf0, /**< U1STAT mask for the endpoint number. */
    USB_ENDPOINT_STALL = 0x02,            /**< Endpoint-control stall bit. */
    USB_ENDPOINT_HANDSHAKE = 0x01,        /**< Endpoint-control handshake-enable bit. */
    USB_ENDPOINT_TRANSMIT = 0x04,         /**< Endpoint-control device-to-host-enable bit. */
    USB_ENDPOINT_RECEIVE = 0x08,          /**< Endpoint-control host-to-device-enable bit. */
    USB_ENDPOINT_CONTROL_DISABLE = 0x10,  /**< Endpoint-control control-transfer-disable bit. */
    USB_ENDPOINT_CONTROL = USB_ENDPOINT_HANDSHAKE | USB_ENDPOINT_TRANSMIT |
                           USB_ENDPOINT_RECEIVE, /**< Default endpoint-zero control flags. */
    USB_ENDPOINT_DIRECTION_IN = 0x80, /**< Endpoint-address device-to-host direction bit. */
    USB_ENDPOINT_NUMBER_MASK = 0x0f,  /**< Endpoint-address number mask. */
    USB_ENDPOINT_ADDRESS_RESERVED_MASK = 0x70, /**< Endpoint-address reserved-bit mask. */
};

/**
 * @brief Delay cycles required before reconnecting the USB controller.
 */
static const uint32_t USB_RESTART_DELAY_CYCLES = 0x9000UL * 0x0c81UL * 2UL;

/**
 * @brief Delay cycles before asserting the USB resume signal.
 */
static const uint32_t USB_RESUME_PREPARE_DELAY_CYCLES = 0x0e10UL;

/**
 * @brief Delay cycles for the USB resume signal pulse.
 */
static const uint32_t USB_RESUME_SIGNAL_DELAY_CYCLES = 0x09c4UL;

/**
 * @brief USB buffer-descriptor table shared with the controller.
 */
static volatile UsbBufferDescriptor descriptors[USB_DESCRIPTOR_COUNT] __attribute__((aligned(512)));

/**
 * @brief USB packet buffers indexed by endpoint, direction, and ping-pong bank.
 */
static volatile uint8_t buffers[USB_ENDPOINT_COUNT][USB_DIRECTION_COUNT][USB_BANK_COUNT]
                               [PLATFORM_USB_PACKET_SIZE];

/**
 * @brief Interrupt-to-foreground USB event ring.
 */
typedef struct {
    PlatformUsbEventType type;
    uint8_t endpoint;
    uint8_t length;
    bool odd_bank;
} PlatformUsbQueuedEvent;

static volatile PlatformUsbQueuedEvent events[USB_EVENT_CAPACITY];

/**
 * @brief Next event-ring insertion index.
 */
static volatile uint8_t event_head;

/**
 * @brief Next event-ring removal index.
 */
static volatile uint8_t event_tail;

/**
 * @brief Next ping-pong bank to try for each endpoint direction.
 *
 * Endpoint zero keeps its selected bank until the controller reports completion so an aborted
 * control transfer can reclaim that bank without changing the hardware topology.
 */
static volatile uint8_t next_bank[USB_ENDPOINT_COUNT][USB_DIRECTION_COUNT];

/**
 * @brief Last completed endpoint-zero output bank.
 */
static volatile uint8_t control_out_current_bank;

/**
 * @brief SOF frames remaining before refreshing the last transaction's descriptor pair.
 */
static volatile uint8_t sof_recovery_frames;

/**
 * @brief Endpoint selected by the SOF descriptor-recovery watchdog.
 */
static volatile uint8_t sof_recovery_endpoint;

/**
 * @brief Direction selected by the SOF descriptor-recovery watchdog.
 */
static volatile bool sof_recovery_input;

/**
 * @brief Whether an endpoint-zero transaction has started the SOF descriptor-recovery watchdog.
 */
static volatile bool sof_recovery_pending;

/** @brief USB device-controller attachment states. */
typedef enum {
    USB_CONTROLLER_DETACHED, /**< Controller is detached from the bus. */
    USB_CONTROLLER_ATTACHED, /**< Controller is attached to the bus. */
} UsbControllerAttachmentState;

/** @brief Current USB device-controller attachment state. */
static UsbControllerAttachmentState controller_attachment_state;

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
 * @brief Refreshes one still-owned descriptor after the SOF watchdog expires.
 *
 * Rewrites transfer fields before ownership, preserving a setup stall guard and restoring a data
 * descriptor to DATA1 while clearing completed-packet bits that the controller may have left in
 * the status word.
 *
 * @param[in,out] target Descriptor to refresh.
 */
static void refresh_owned_descriptor(volatile UsbBufferDescriptor *target) {
    if (!usb_buffer_descriptor_owned(target)) {
        return;
    }
    uint16_t status = target->status;
    uint16_t count = target->count & USB_BUFFER_COUNT_MASK;
    uint16_t address = target->address;
    uint16_t address_high = target->address_high;
    target->address = address;
    target->address_high = address_high;
    target->count = count;
    uint16_t mode = status & USB_BUFFER_DATA_TOGGLE_ENABLED;
    uint16_t stall = mode == 0 ? status & USB_BUFFER_STALL : 0;
    target->status =
        mode | (mode != 0 ? USB_BUFFER_DATA_TOGGLE : 0) | stall | USB_BUFFER_OWNED_BY_USB;
}

/**
 * @brief Cancels the SOF descriptor-recovery watchdog.
 */
static void clear_sof_recovery(void) {
    sof_recovery_frames = 0;
    sof_recovery_endpoint = 0;
    sof_recovery_input = false;
    sof_recovery_pending = false;
}

/**
 * @brief Refreshes the descriptor pair associated with the last endpoint-zero transaction.
 *
 * The controller's official recovery path rewrites the active endpoint-direction pair after 45
 * SOF frames without releasing ownership, allowing a stalled hardware descriptor to be retried.
 */
static void recover_stuck_descriptors(void) {
    if (!sof_recovery_pending) {
        return;
    }
    refresh_owned_descriptor(descriptor(sof_recovery_endpoint, sof_recovery_input, false));
    refresh_owned_descriptor(descriptor(sof_recovery_endpoint, sof_recovery_input, true));
}

/**
 * @brief Services one start-of-frame watchdog tick.
 */
static void handle_start_of_frame(void) {
    if (sof_recovery_pending && sof_recovery_frames != 0) {
        sof_recovery_frames--;
    }
    if (sof_recovery_pending && sof_recovery_frames == 0) {
        recover_stuck_descriptors();
        sof_recovery_frames = 0;
        sof_recovery_pending = false;
    }
}

/**
 * @brief Queues one USB controller event.
 *
 * Copies the completed packet into the interrupt-to-main event ring and drops the event when the
 * ring's usable slots are full.
 *
 * @param[in] type Controller event type.
 * @param[in] endpoint Endpoint associated with the event.
 * @param[in] data Completed packet bytes, or null for events without a payload.
 * @param[in] length Number of packet bytes to copy.
 */
static void push_event(PlatformUsbEventType type, uint8_t endpoint, bool odd_bank, uint8_t length) {
    uint8_t next = event_head + 1;
    if (next == USB_EVENT_CAPACITY) {
        next = 0;
    }
    if (next == event_tail) {
        return;
    }

    volatile PlatformUsbQueuedEvent *event = &events[event_head];
    event->type = type;
    event->endpoint = endpoint;
    event->length = length;
    event->odd_bank = odd_bank;
    event_head = next;
}

/**
 * @brief Restores the USB controller to its default device state.
 *
 * Clears pending controller events, resets ping-pong selection, address zero, endpoint controls,
 * queued events, descriptors, and both endpoint-zero setup banks before allowing token processing.
 *
 */
static void reset_controller(void) {
    U1EIR = 0xff;
    U1IR = 0xff;
    while (U1IRbits.TRNIF != 0) {
        U1IR = USB_INTERRUPT_TRANSACTION;
    }
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
    control_out_current_bank = 0;
    clear_sof_recovery();
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
 * state, and records the detached state.
 *
 */
void platform_usb_init(void) {
    IEC5bits.USB1IE = 0;
    controller_attachment_state = USB_CONTROLLER_DETACHED;
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
    U1IE = 0x9f;
    U1PWRCbits.USBPWR = 1;
    U1BDTP1 = (uint16_t)((uint16_t)&descriptors[0] >> 8);
    reset_controller();
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
 * Leaves an active attachment unchanged. From the detached state, resets controller and event
 * configuration, enables the supported device events and the priority-four interrupt, asserts the
 * USB transceiver until the controller reports it enabled, and records the attached state.
 *
 */
void platform_usb_attach(void) {
    if (controller_attachment_state != USB_CONTROLLER_DETACHED) {
        return;
    }
    U1CON = 0;
    U1IE = 0;
    U1CNFG1 = 0;
    U1EIE = 0x9f;
    U1IE = 0x9f;
    IEC5bits.USB1IE = 1;
    IPC21bits.USB1IP = USB_INTERRUPT_PRIORITY;
    while (U1CONbits.USBEN == 0) {
        U1CONbits.USBEN = 1;
    }
    controller_attachment_state = USB_CONTROLLER_ATTACHED;
}

/**
 * @brief Detaches the USB device controller from the bus.
 *
 * Disables the CPU interrupt, USB transceiver, and controller event sources.
 *
 */
void platform_usb_detach(void) {
    IEC5bits.USB1IE = 0;
    U1CON = 0;
    U1IE = 0;
    clear_sof_recovery();
    controller_attachment_state = USB_CONTROLLER_DETACHED;
}

/**
 * @brief Restarts the USB device controller after its reattachment delay.
 *
 * Disables the controller, discards queued transfers, waits for 0x9000 groups of 0x0c81
 * two-cycle delay operations, resets all endpoint banks, and reconnects to the bus.
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
 * @brief Signals USB resume to the host.
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
        const volatile PlatformUsbQueuedEvent *source = &events[event_tail];
        event->type = source->type;
        event->endpoint = source->endpoint;
        event->length = source->length;
        if (source->type == PLATFORM_USB_EVENT_SETUP || source->type == PLATFORM_USB_EVENT_OUT) {
            const volatile uint8_t *data =
                &buffers[source->endpoint][0][source->odd_bank ? 1 : 0][0];
            for (uint8_t index = 0; index < source->length; index++) {
                event->data[index] = data[index];
            }
        }
        event_tail++;
        if (event_tail == USB_EVENT_CAPACITY) {
            event_tail = 0;
        }
    }
    IEC5bits.USB1IE = interrupt_enabled;
    return available;
}

/**
 * @brief Arms one USB endpoint transfer.
 *
 * Selects the next available ping-pong bank, reclaims a prearmed endpoint-zero setup bank when
 * token processing is gated, copies device-to-host bytes, publishes the descriptor to the
 * controller, advances non-control bank selection, and releases endpoint-zero token processing
 * only after the requested control stage is ready. Endpoint-zero selection advances when the
 * controller reports the completed bank.
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
        if (usb_buffer_descriptor_owned(target)) {
            if (!replace_setup) {
                continue;
            }
            target->status &= (uint16_t)~USB_BUFFER_OWNED_BY_USB;
        }
        if (input) {
            volatile uint8_t *destination = &buffers[endpoint][1][bank][0];
            for (uint8_t index = 0; index < length; index++) {
                destination[index] = data[index];
            }
        }
        usb_buffer_descriptor_arm(target, buffer_address(endpoint, input, bank != 0), length,
                                  data_one, false);
        if (endpoint != 0) {
            next_bank[endpoint][direction] = bank ^ 1;
        }
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
    clear_sof_recovery();
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
 * @brief Resets endpoint-zero state for a new setup transaction.
 *
 * Releases the input banks and the controller's next output bank without changing either ping-pong
 * selector. The following control-stage arm can therefore reuse the bank topology left by the
 * completed setup transaction.
 */
void platform_usb_control_reset(void) {
    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    clear_sof_recovery();
    for (uint8_t bank = 0; bank < USB_BANK_COUNT; bank++) {
        volatile UsbBufferDescriptor *target = descriptor(0, true, bank != 0);
        target->status &= (uint16_t)~USB_BUFFER_OWNED_BY_USB;
    }
    volatile UsbBufferDescriptor *target = descriptor(0, false, next_bank[0][0] != 0);
    target->status &= (uint16_t)~USB_BUFFER_OWNED_BY_USB;
    U1EP0bits.EPSTALL = 0;
    IEC5bits.USB1IE = interrupt_enabled;
}

/**
 * @brief Arms the official endpoint-zero status descriptors.
 *
 * Input status uses a DATA1 zero-length packet. Output status uses the bank following the last
 * completed output bank and keeps the completed bank as a stalled setup guard.
 */
bool platform_usb_control_arm_status(bool input) {
    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    if (input) {
        uint8_t bank = next_bank[0][1];
        volatile UsbBufferDescriptor *target = descriptor(0, true, bank != 0);
        if (usb_buffer_descriptor_owned(target)) {
            IEC5bits.USB1IE = interrupt_enabled;
            return false;
        }
        usb_buffer_descriptor_arm(target, buffer_address(0, true, bank != 0), 0, true, false);
    } else {
        uint8_t status_bank = next_bank[0][0];
        uint8_t guard_bank = control_out_current_bank;
        volatile UsbBufferDescriptor *guard = descriptor(0, false, guard_bank != 0);
        volatile UsbBufferDescriptor *status = descriptor(0, false, status_bank != 0);
        if (usb_buffer_descriptor_owned(guard) || usb_buffer_descriptor_owned(status)) {
            IEC5bits.USB1IE = interrupt_enabled;
            return false;
        }
        arm_setup_bank(guard_bank != 0);
        status->address = buffer_address(0, false, status_bank != 0);
        status->address_high = 0;
        status->count = PLATFORM_USB_PACKET_SIZE;
        status->status = USB_BUFFER_OWNED_BY_USB;
    }
    U1CONbits.PKTDIS = 0;
    IEC5bits.USB1IE = interrupt_enabled;
    return true;
}

/**
 * @brief Resets endpoint ping-pong state after a configuration change.
 *
 * Disables non-control endpoints, clears all descriptors, and pulses PPBRST so the controller and
 * software use bank zero for the next transfer in every direction.
 */
void platform_usb_reset_endpoint_state(void) {
    bool interrupt_enabled = IEC5bits.USB1IE != 0;
    IEC5bits.USB1IE = 0;
    for (uint8_t endpoint = 1; endpoint < USB_ENDPOINT_COUNT; endpoint++) {
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control = 0;
    }
    clear_descriptors();
    U1CONbits.PPBRST = 1;
    for (uint8_t endpoint = 0; endpoint < USB_ENDPOINT_COUNT; endpoint++) {
        next_bank[endpoint][0] = 0;
        next_bank[endpoint][1] = 0;
    }
    control_out_current_bank = 0;
    clear_sof_recovery();
    U1CONbits.PPBRST = 0;
    IEC5bits.USB1IE = interrupt_enabled;
}

/**
 * @brief Applies the enumerated USB device address.
 *
 * Writes the low seven address bits to the controller.
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
        bool interrupt_enabled = IEC5bits.USB1IE != 0;
        IEC5bits.USB1IE = 0;
        if (endpoint == 0) {
            clear_sof_recovery();
            uint8_t output_bank = next_bank[0][0];
            volatile UsbBufferDescriptor *output = descriptor(0, false, output_bank != 0);
            usb_buffer_descriptor_arm(output, buffer_address(0, false, output_bank != 0),
                                      PLATFORM_USB_PACKET_SIZE, false, true);
            output->status =
                USB_BUFFER_STALL | USB_BUFFER_DATA_TOGGLE_ENABLED | USB_BUFFER_OWNED_BY_USB;
            volatile UsbBufferDescriptor *input = descriptor(0, true, next_bank[0][1] != 0);
            input->status = USB_BUFFER_STALL | USB_BUFFER_OWNED_BY_USB;
        }
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control |= USB_ENDPOINT_STALL;
        if (endpoint == 0) {
            U1CONbits.PKTDIS = 0;
        }
        IEC5bits.USB1IE = interrupt_enabled;
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
 * A set operation presents a stall on both banks. A clear operation releases both banks,
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
        usb_buffer_descriptor_set_halt(descriptor(endpoint, input, !odd_bank));
    } else {
        usb_buffer_descriptor_clear_halt(selected, descriptor(endpoint, input, !odd_bank));
        volatile uint16_t *endpoint_control = &U1EP0 + endpoint;
        *endpoint_control &= (uint16_t)~USB_ENDPOINT_STALL;
    }
    IEC5bits.USB1IE = interrupt_enabled;
}

#ifdef OPENTEC_SIMULATOR_TEST
uint16_t platform_usb_test_descriptor_status(uint8_t endpoint, bool input, bool odd_bank) {
    return endpoint < USB_ENDPOINT_COUNT ? descriptor(endpoint, input, odd_bank)->status : 0;
}

uint16_t platform_usb_test_descriptor_count(uint8_t endpoint, bool input, bool odd_bank) {
    return endpoint < USB_ENDPOINT_COUNT ? descriptor(endpoint, input, odd_bank)->count : 0;
}

void platform_usb_test_set_descriptor(uint8_t endpoint, bool input, bool odd_bank, uint16_t status,
                                      uint16_t count) {
    if (endpoint < USB_ENDPOINT_COUNT) {
        volatile UsbBufferDescriptor *target = descriptor(endpoint, input, odd_bank);
        target->status = status;
        target->count = count;
    }
}
#endif

/**
 * @brief Captures one completed USB transaction.
 *
 * Decodes endpoint, direction, and ping-pong bank from controller status, advances the bank
 * selector, and queues the completed setup, output, or input-completion event. Setup traffic clears
 * a prior control stall but remains token-gated until its next stage is armed.
 *
 */
static void handle_transaction(uint8_t status) {
    uint8_t endpoint = (status & USB_TRANSACTION_ENDPOINT_MASK) >> 4;
    bool input = (status & USB_TRANSACTION_INPUT) != 0;
    bool odd_bank = (status & USB_TRANSACTION_ODD_BANK) != 0;
    if (endpoint >= USB_ENDPOINT_COUNT) {
        return;
    }
    next_bank[endpoint][input ? 1 : 0] = odd_bank ? 0 : 1;
    if (endpoint == 0) {
        sof_recovery_endpoint = endpoint;
        sof_recovery_input = input;
        sof_recovery_frames = USB_SOF_RECOVERY_FRAME_COUNT;
        sof_recovery_pending = true;
    }
    volatile UsbBufferDescriptor *completed = descriptor(endpoint, input, odd_bank);
    uint8_t length = (uint8_t)usb_buffer_descriptor_count(completed);

    if (input) {
        push_event(PLATFORM_USB_EVENT_IN_COMPLETE, endpoint, odd_bank, 0);
        return;
    }

    if (endpoint == 0) {
        control_out_current_bank = odd_bank;
    }
    PlatformUsbEventType type = usb_buffer_descriptor_packet_id(completed) == USB_PACKET_ID_SETUP
                                    ? PLATFORM_USB_EVENT_SETUP
                                    : PLATFORM_USB_EVENT_OUT;
    push_event(type, endpoint, odd_bank, length);
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
    if (U1OTGIEbits.ACTVIE != 0 && U1OTGIRbits.ACTVIF != 0) {
        U1OTGIRbits.ACTVIF = 1;
        U1OTGIEbits.ACTVIE = 0;
        U1PWRCbits.USUSPEND = 0;
    }
    if (U1PWRCbits.USUSPEND != 0) {
        IFS5bits.USB1IF = 0;
        return;
    }
    if (U1IRbits.URSTIF != 0 && (U1IE & USB_INTERRUPT_RESET) != 0) {
        reset_controller();
        push_event(PLATFORM_USB_EVENT_RESET, 0, false, 0);
        U1IR = USB_INTERRUPT_RESET;
    }
    if (U1IRbits.IDLEIF != 0 && (U1IE & USB_INTERRUPT_IDLE) != 0) {
        push_event(PLATFORM_USB_EVENT_SUSPEND, 0, false, 0);
        U1IR = USB_INTERRUPT_IDLE;
        U1OTGIRbits.ACTVIF = 1;
        U1OTGIEbits.ACTVIE = 1;
        U1PWRCbits.USUSPEND = 1;
    }
    if (U1IRbits.UERRIF != 0 && (U1IE & USB_INTERRUPT_ERROR) != 0) {
        U1EIR = 0xff;
        U1IR = USB_INTERRUPT_ERROR;
    }
    if (U1IRbits.SOFIF != 0 && (U1IE & USB_INTERRUPT_SOF) != 0) {
        U1IR = USB_INTERRUPT_SOF;
        handle_start_of_frame();
    }
    if (U1IRbits.STALLIF != 0 && (U1IE & USB_INTERRUPT_STALL) != 0) {
        if (U1EP0bits.EPSTALL != 0) {
            volatile UsbBufferDescriptor *output =
                descriptor(0, false, control_out_current_bank != 0);
            volatile UsbBufferDescriptor *input = descriptor(0, true, next_bank[0][1] != 0);
            if (output->status == USB_BUFFER_OWNED_BY_USB &&
                input->status == (USB_BUFFER_STALL | USB_BUFFER_OWNED_BY_USB)) {
                output->status =
                    USB_BUFFER_STALL | USB_BUFFER_DATA_TOGGLE_ENABLED | USB_BUFFER_OWNED_BY_USB;
                output->count = (uint16_t)(output->count & 0x00ff);
            }
            U1EP0bits.EPSTALL = 0;
        }
        U1IR = USB_INTERRUPT_STALL;
    }
    for (uint8_t transaction = 0;
         transaction < 4 && U1IRbits.TRNIF != 0 && (U1IE & USB_INTERRUPT_TRANSACTION) != 0;
         transaction++) {
        handle_transaction(U1STAT);
        U1IR = USB_INTERRUPT_TRANSACTION;
    }
    IFS5bits.USB1IF = 0;
}

#ifdef OPENTEC_SIMULATOR_TEST
/**
 * @brief Services a synthetic transaction-complete source for platform tests.
 *
 * The simulator models U1STAT and U1IR as hardware-owned registers, so tests inject their decoded
 * values through this production-path helper while retaining the real U1IE gate.
 *
 * @param[in] status Synthetic U1STAT endpoint, direction, and bank value.
 */
void platform_usb_test_service_transaction(uint8_t status) {
    if ((U1IE & USB_INTERRUPT_TRANSACTION) != 0) {
        handle_transaction(status);
    }
}

/**
 * @brief Services a synthetic start-of-frame source for platform tests.
 *
 * @return None.
 */
void platform_usb_test_service_sof(void) {
    if ((U1IE & USB_INTERRUPT_SOF) != 0) {
        handle_start_of_frame();
    }
}

#endif
