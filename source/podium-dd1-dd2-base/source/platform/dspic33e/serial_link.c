#include "platform/serial_link.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xc.h>

/**
 * @brief Framed and direct-mode serial-link hardware settings.
 */
enum {
    SERIAL_LINK_TRANSMIT_SIZE = 72, /**< Number of bytes sent by one framed DMA transfer. */
    SERIAL_LINK_RECEIVE_SIZE = 68,  /**< Number of bytes received by one framed DMA transfer. */
    SERIAL_LINK_FRAME_OFFSET = 4, /**< Offset of the transport packet within the transmit buffer. */
    SERIAL_LINK_ALIGNMENT_LIMIT =
        5, /**< Number of leading receive offsets searched for a frame marker. */
    SERIAL_LINK_PADDING = 0xf0,  /**< Padding byte surrounding a framed transport packet. */
    SERIAL_LINK_BAUD_PERIOD = 2, /**< UART3 baud-period register value for framed mode. */
    SERIAL_LINK_TRANSMIT_DMA_REQUEST = 0x53,   /**< DMA request number for UART3 transmission. */
    SERIAL_LINK_RECEIVE_DMA_REQUEST = 0x52,    /**< DMA request number for UART3 reception. */
    SERIAL_LINK_TRANSMIT_PRIORITY = 5,         /**< Framed transmit-DMA interrupt priority. */
    SERIAL_LINK_RECEIVE_PRIORITY = 4,          /**< Framed receive-DMA interrupt priority. */
    SERIAL_LINK_TIMER_PRIORITY = 6,            /**< Serial-link Timer 6 interrupt priority. */
    SERIAL_LINK_TRANSMIT_GUARD_PERIOD = 0x4b0, /**< Delay before arming framed receive DMA. */
    SERIAL_LINK_RECEIVE_TIMEOUT_PERIOD =
        10000, /**< Timer period for an incomplete framed response. */
    SERIAL_LINK_DIRECT_TRANSMIT_CAPACITY = 63, /**< Maximum queued direct-mode request bytes. */
    SERIAL_LINK_DIRECT_RECEIVE_CAPACITY = 66,  /**< Maximum retained direct-mode response bytes. */
    SERIAL_LINK_DIRECT_BAUD_PERIOD = 0xc2, /**< UART3 baud-period register value for direct mode. */
    SERIAL_LINK_DIRECT_SERVICE_PERIOD = 600, /**< Direct-mode service timer period. */
    SERIAL_LINK_DIRECT_TURNAROUND_TICKS = 2, /**< Direct-mode receive-turnaround service ticks. */
};

/**
 * @brief Action selected for Timer 6 completion.
 */
typedef enum {
    SERIAL_LINK_TIMER_IDLE,            /**< No Timer 6 action is pending. */
    SERIAL_LINK_TIMER_RECEIVE_TIMEOUT, /**< Abort an incomplete framed receive. */
    SERIAL_LINK_TIMER_START_RECEIVE,   /**< Arm framed receive DMA after transmit guard time. */
    SERIAL_LINK_TIMER_DIRECT_SERVICE,  /**< Advance direct-mode transmission or reception. */
} SerialLinkTimerAction;

/**
 * @brief Phase of the direct-mode serial exchange.
 */
typedef enum {
    SERIAL_LINK_DIRECT_IDLE, /**< Ready to start a queued request or collect response bytes. */
    SERIAL_LINK_DIRECT_TRANSMITTING, /**< A queued request is being transmitted. */
    SERIAL_LINK_DIRECT_TURNAROUND,   /**< Waiting before enabling response reception. */
} SerialLinkDirectPhase;

/**
 * @brief UART3 transmit DMA storage for framed transfers.
 */
static volatile uint8_t transmit_dma[SERIAL_LINK_TRANSMIT_SIZE];

/**
 * @brief UART3 receive DMA storage for framed transfers.
 */
static volatile uint8_t receive_dma[SERIAL_LINK_RECEIVE_SIZE];

/**
 * @brief Most recently aligned received transport packet.
 */
static volatile uint8_t received_packet[SERIAL_PACKET_SIZE];

/**
 * @brief True while a framed request and response exchange is active.
 */
static volatile bool transfer_active;

/**
 * @brief True when an aligned framed response is ready for the foreground.
 */
static volatile bool frame_ready;

/**
 * @brief Timer 6 completion action currently scheduled.
 */
static volatile SerialLinkTimerAction timer_action;

/**
 * @brief Direct-mode transmit buffer.
 */
static volatile uint8_t direct_transmit[SERIAL_LINK_DIRECT_TRANSMIT_CAPACITY];

/**
 * @brief Direct-mode receive buffer.
 */
static volatile uint8_t direct_receive[SERIAL_LINK_DIRECT_RECEIVE_CAPACITY];

/**
 * @brief Number of direct-mode transmit bytes pending.
 */
static volatile uint8_t direct_transmit_length;

/**
 * @brief Index of the next direct-mode transmit byte.
 */
static volatile uint8_t direct_transmit_index;

/**
 * @brief Number of direct-mode response bytes retained.
 */
static volatile uint8_t direct_receive_length;

/**
 * @brief Remaining direct-mode turnaround service ticks.
 */
static volatile uint8_t direct_turnaround_ticks;

/**
 * @brief True after the link has entered direct updater mode.
 */
static volatile bool direct_mode;

/**
 * @brief True while a direct-mode request is being transmitted.
 */
static volatile bool direct_transmit_active;

/**
 * @brief Current direct-mode serial phase.
 */
static volatile SerialLinkDirectPhase direct_phase;

/**
 * @brief Clears UART3 receive errors and pending bytes.
 *
 * Clears overrun, framing, and parity state before draining the receive FIFO.
 *
 */
static void clear_uart(void) {
    U3STAbits.OERR = 0;
    U3STAbits.FERR = 0;
    U3STAbits.PERR = 0;
    while (U3STAbits.URXDA != 0) {
        (void)U3RXREG;
    }
}

/**
 * @brief Configures UART3 for the normal attached-device link.
 *
 * The link uses inverted receive and transmit signals, high-speed baud generation, and a baud
 * period of 2.
 *
 */
static void configure_uart(void) {
    U3MODEbits.UARTEN = 0;
    U3MODE = 0;
    U3STA = 0;
    U3MODEbits.BRGH = 1;
    U3MODEbits.URXINV = 1;
    U3BRG = SERIAL_LINK_BAUD_PERIOD;
    U3STAbits.UTXINV = 1;
    U3MODEbits.UARTEN = 1;
    U3STAbits.UTXEN = 1;
}

/**
 * @brief Configures the byte-oriented, one-shot UART3 DMA channels.
 *
 * DMA5 sends 72 bytes with request 0x53. DMA6 receives 68 bytes with request 0x52.
 *
 */
static void configure_dma(void) {
    DMA5CON = 0;
    DMA5CONbits.SIZE = 1;
    DMA5CONbits.DIR = 1;
    DMA5CONbits.AMODE = 0;
    DMA5CONbits.MODE = 1;
    DMA5REQbits.IRQSEL = SERIAL_LINK_TRANSMIT_DMA_REQUEST;
    DMA5PAD = (uint16_t)&U3TXREG;
    DMA5STAL = (uint16_t)transmit_dma;
    DMA5STAH = 0;
    DMA5CNT = SERIAL_LINK_TRANSMIT_SIZE - 1;

    DMA6CON = 0;
    DMA6CONbits.SIZE = 1;
    DMA6CONbits.DIR = 0;
    DMA6CONbits.AMODE = 0;
    DMA6CONbits.MODE = 1;
    DMA6REQbits.IRQSEL = SERIAL_LINK_RECEIVE_DMA_REQUEST;
    DMA6PAD = (uint16_t)&U3RXREG;
    DMA6STAL = (uint16_t)receive_dma;
    DMA6STAH = 0;
    DMA6CNT = SERIAL_LINK_RECEIVE_SIZE - 1;
}

/**
 * @brief Configures attached-device link interrupts.
 *
 * DMA5 uses priority 5, DMA6 uses priority 4, and Timer 6 uses priority 6.
 *
 */
static void configure_interrupts(void) {
    IPC15bits.DMA5IP = SERIAL_LINK_TRANSMIT_PRIORITY;
    IPC17bits.DMA6IP = SERIAL_LINK_RECEIVE_PRIORITY;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    IFS2bits.T6IF = 0;
    IFS5bits.U3EIF = 0;
    IFS5bits.U3RXIF = 0;
    IEC3bits.DMA5IE = 1;
    IEC4bits.DMA6IE = 1;
    IPC11bits.T6IP = SERIAL_LINK_TIMER_PRIORITY;
    IEC2bits.T6IE = 1;
    IEC5bits.U3EIE = 1;
}

/**
 * @brief Configures Timer 6 for attached-device link timing.
 *
 * Selects the internal clock with a 1:1 prescaler and leaves the timer stopped.
 *
 */
static void configure_timer(void) {
    T6CON = 0;
    TMR6 = 0;
    PR6 = 0;
    timer_action = SERIAL_LINK_TIMER_IDLE;
}

/**
 * @brief Starts an attached-device link timing interval.
 *
 * Replaces the current Timer 6 interval with the supplied period and completion action.
 *
 * @param[in] period Timer 6 period in instruction cycles.
 * @param[in] action Action to perform when the period expires.
 */
static void start_timer(uint16_t period, SerialLinkTimerAction action) {
    T6CONbits.TON = 0;
    timer_action = action;
    TMR6 = 0;
    PR6 = period;
    IFS2bits.T6IF = 0;
    T6CONbits.TON = 1;
}

/**
 * @brief Stops attached-device link timing.
 *
 * Stops Timer 6, clears its counter and pending interrupt, and removes the scheduled action.
 *
 */
static void stop_timer(void) {
    T6CONbits.TON = 0;
    TMR6 = 0;
    IFS2bits.T6IF = 0;
    timer_action = SERIAL_LINK_TIMER_IDLE;
}

/**
 * @brief Arms UART3 receive DMA for one 68-byte transfer.
 *
 * Clears the receive storage and UART state before enabling DMA6 and the first-byte interrupt.
 *
 */
static void start_receive(void) {
    DMA6CONbits.CHEN = 0;
    IEC4bits.DMA6IE = 0;
    for (uint8_t index = 0; index < SERIAL_LINK_RECEIVE_SIZE; index++) {
        receive_dma[index] = 0;
    }
    clear_uart();
    IFS4bits.DMA6IF = 0;
    IFS5bits.U3RXIF = 0;
    DMA6CONbits.CHEN = 1;
    IEC4bits.DMA6IE = 1;
    IEC5bits.U3RXIE = 1;
}

/**
 * @brief Advances the raw UART turnaround state.
 *
 * Starts a queued write, waits for its interrupt-driven completion, then holds receive disabled
 * for two 600-cycle service periods and waits for receiver idle before accepting response bytes.
 *
 */
static void service_direct_mode(void) {
    if (direct_phase == SERIAL_LINK_DIRECT_IDLE) {
        if (direct_transmit_length == 0) {
            return;
        }
        IEC5bits.U3RXIE = 0;
        direct_transmit_active = true;
        direct_transmit_index = 1;
        U3TXREG = direct_transmit[0];
        direct_phase = SERIAL_LINK_DIRECT_TRANSMITTING;
        return;
    }

    if (direct_phase == SERIAL_LINK_DIRECT_TRANSMITTING) {
        if (direct_transmit_active) {
            return;
        }
        direct_transmit_length = 0;
        direct_turnaround_ticks = SERIAL_LINK_DIRECT_TURNAROUND_TICKS;
        direct_phase = SERIAL_LINK_DIRECT_TURNAROUND;
        return;
    }

    if (direct_turnaround_ticks != 0) {
        direct_turnaround_ticks--;
        if (direct_turnaround_ticks != 0) {
            return;
        }
    }
    direct_receive_length = 0;
    while (U3STAbits.RIDLE == 0) {
        __builtin_nop();
    }
    clear_uart();
    IFS5bits.U3RXIF = 0;
    IEC5bits.U3RXIE = 1;
    direct_phase = SERIAL_LINK_DIRECT_IDLE;
}

/**
 * @brief Initializes the attached-device UART3 exchange layer.
 *
 * Configures the physical pins, inverted high-speed UART, one-shot DMA channels, and Timer 6.
 *
 */
void platform_serial_link_init(void) {
    direct_mode = false;
    direct_transmit_active = false;
    direct_transmit_length = 0;
    direct_receive_length = 0;
    transfer_active = false;
    frame_ready = false;
    TRISFbits.TRISF2 = 0;
    TRISFbits.TRISF8 = 1;
    CNPDFbits.CNPDF2 = 1;
    CNPDFbits.CNPDF8 = 1;
    configure_uart();
    configure_dma();
    configure_timer();
    configure_interrupts();
    clear_uart();
}

/**
 * @brief Resets the attached-device UART3 exchange layer.
 *
 * Stops all active DMA and timer work, clears pending receive state, and releases the transaction.
 *
 */
void platform_serial_link_reset(void) {
    if (direct_mode) {
        return;
    }
    IEC3bits.DMA5IE = 0;
    IEC4bits.DMA6IE = 0;
    IEC5bits.U3RXIE = 0;
    DMA5CONbits.CHEN = 0;
    DMA6CONbits.CHEN = 0;
    DMA5CON = 0;
    DMA5REQ = 0;
    DMA5PAD = 0;
    DMA5STAL = 0;
    DMA5STAH = 0;
    DMA5CNT = 0;
    DMA6CON = 0;
    DMA6REQ = 0;
    DMA6PAD = 0;
    DMA6STAL = 0;
    DMA6STAH = 0;
    DMA6CNT = 0;
    stop_timer();
    clear_uart();
    transfer_active = false;
    frame_ready = false;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    IEC3bits.DMA5IE = 1;
    IEC4bits.DMA6IE = 1;
}

/**
 * @brief Starts one attached-device request and response exchange.
 *
 * Places the 64-byte transport frame between four leading and trailing padding bytes, then starts
 * the transmit DMA. Framed mode accepts only one exchange at a time.
 *
 * @param[in] packet Transport packet to transmit.
 * @return true when the exchange starts; false when another exchange is active.
 */
bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    IEC3bits.DMA5IE = 0;
    if (direct_mode || transfer_active) {
        IEC3bits.DMA5IE = 1;
        return false;
    }
    for (uint8_t index = 0; index < SERIAL_LINK_TRANSMIT_SIZE; index++) {
        transmit_dma[index] = SERIAL_LINK_PADDING;
    }
    for (uint8_t index = 0; index < SERIAL_PACKET_SIZE; index++) {
        transmit_dma[SERIAL_LINK_FRAME_OFFSET + index] = packet[index];
    }
    DMA5CONbits.CHEN = 0;
    DMA6CONbits.CHEN = 0;
    IEC5bits.U3RXIE = 0;
    stop_timer();
    clear_uart();
    frame_ready = false;
    transfer_active = true;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    DMA5CONbits.CHEN = 1;
    DMA5REQbits.FORCE = 1;
    IEC3bits.DMA5IE = 1;
    return true;
}

/**
 * @brief Takes the completed attached-device response frame.
 *
 * Copies a newly aligned 64-byte response into caller storage and consumes its ready state. The
 * operation returns false while direct mode is active.
 *
 * @param[out] packet Storage that receives the transport packet.
 * @return true when a frame was copied; false when no response is ready.
 */
bool platform_serial_link_take_received(uint8_t packet[SERIAL_PACKET_SIZE]) {
    if (direct_mode) {
        return false;
    }
    IEC4bits.DMA6IE = 0;
    bool ready = frame_ready;
    if (ready) {
        for (uint8_t index = 0; index < SERIAL_PACKET_SIZE; index++) {
            packet[index] = received_packet[index];
        }
        frame_ready = false;
    }
    IEC4bits.DMA6IE = 1;
    return ready;
}

/**
 * @brief Selects the raw attached-wheel updater link.
 *
 * Stops framed DMA exchanges and configures UART3 for inverted 8-N-1 traffic with baud period
 * 0xc2. A 600-cycle periodic service drives transmission and the two-period receive turnaround.
 *
 */
void platform_serial_link_enter_direct_mode(void) {
    IEC3bits.DMA5IE = 0;
    IEC4bits.DMA6IE = 0;
    IEC5bits.U3TXIE = 0;
    IEC5bits.U3RXIE = 0;
    DMA5CONbits.CHEN = 0;
    DMA6CONbits.CHEN = 0;
    stop_timer();
    U3STAbits.UTXEN = 0;
    U3MODEbits.UARTEN = 0;

    transfer_active = false;
    frame_ready = false;
    direct_transmit_active = false;
    direct_transmit_length = 0;
    direct_transmit_index = 0;
    direct_receive_length = 0;
    direct_turnaround_ticks = 0;
    direct_phase = SERIAL_LINK_DIRECT_TURNAROUND;
    direct_mode = true;

    U3MODE = 0;
    U3STA = 0;
    U3MODEbits.URXINV = 1;
    U3BRG = SERIAL_LINK_DIRECT_BAUD_PERIOD;
    U3STAbits.UTXINV = 1;
    U3STAbits.UTXISEL0 = 1;
    U3STAbits.UTXISEL1 = 0;
    U3STAbits.URXISEL0 = 0;
    U3STAbits.URXISEL1 = 0;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    IFS5bits.U3TXIF = 0;
    IFS5bits.U3RXIF = 0;
    IFS5bits.U3EIF = 0;
    IEC5bits.U3TXIE = 1;
    IEC5bits.U3EIE = 1;
    U3MODEbits.UARTEN = 1;
    U3STAbits.UTXEN = 1;
    start_timer(SERIAL_LINK_DIRECT_SERVICE_PERIOD, SERIAL_LINK_TIMER_DIRECT_SERVICE);
}

/**
 * @brief Queues raw updater request bytes.
 *
 * Retains one nonempty request of up to 63 bytes for the periodic direct-mode transmitter. A
 * request remains pending until UART interrupts send every byte and the service releases its
 * length.
 *
 * @param[in] data Request bytes to transmit.
 * @param[in] length Request byte count.
 * @return true when the request was queued; false when direct mode is inactive or busy.
 */
bool platform_serial_link_direct_write(const uint8_t *data, uint8_t length) {
    if (!direct_mode || data == NULL || length == 0 ||
        length > SERIAL_LINK_DIRECT_TRANSMIT_CAPACITY || direct_transmit_length != 0) {
        return false;
    }
    for (uint8_t index = 0; index < length; index++) {
        direct_transmit[index] = data[index];
    }
    direct_transmit_length = length;
    return true;
}

/**
 * @brief Takes a requested number of raw updater response bytes.
 *
 * Copies and consumes the oldest received bytes only in direct mode and when the complete requested
 * fragment is available. Up to 66 valid response bytes are retained.
 *
 * @param[out] data Storage that receives the requested response fragment.
 * @param[in] length Requested response byte count.
 * @return true when the complete fragment was copied; false when insufficient data is available.
 */
bool platform_serial_link_direct_read(uint8_t *data, uint8_t length) {
    if (!direct_mode || data == NULL || length == 0) {
        return false;
    }
    IEC5bits.U3RXIE = 0;
    if (direct_receive_length < length) {
        if (direct_phase == SERIAL_LINK_DIRECT_IDLE) {
            IEC5bits.U3RXIE = 1;
        }
        return false;
    }
    for (uint8_t index = 0; index < length; index++) {
        data[index] = direct_receive[index];
    }
    direct_receive_length -= length;
    for (uint8_t index = 0; index < direct_receive_length; index++) {
        direct_receive[index] = direct_receive[index + length];
    }
    if (direct_phase == SERIAL_LINK_DIRECT_IDLE) {
        IEC5bits.U3RXIE = 1;
    }
    return true;
}

/**
 * @brief Clears retained direct-mode response bytes.
 */
void platform_serial_link_direct_clear(void) {
    IEC5bits.U3RXIE = 0;
    direct_receive_length = 0;
    if (direct_phase == SERIAL_LINK_DIRECT_IDLE) {
        IEC5bits.U3RXIE = 1;
    }
}

/**
 * @brief Handles completion of the 72-byte UART3 transmit DMA.
 *
 * Stops DMA5 and schedules receive DMA after the 0x4b0-cycle turnaround guard.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _DMA5Interrupt(void) {
    DMA5CONbits.CHEN = 0;
    IFS3bits.DMA5IF = 0;
    start_timer(SERIAL_LINK_TRANSMIT_GUARD_PERIOD, SERIAL_LINK_TIMER_START_RECEIVE);
}

/**
 * @brief Handles completion of the 68-byte UART3 receive DMA.
 *
 * Stops receive timing, aligns the first frame marker within the five accepted offsets, and makes
 * the response available to the service layer.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _DMA6Interrupt(void) {
    DMA6CONbits.CHEN = 0;
    IEC4bits.DMA6IE = 0;
    stop_timer();
    uint8_t offset = 0;
    while (offset < SERIAL_LINK_ALIGNMENT_LIMIT && receive_dma[offset] != SERIAL_PACKET_START) {
        offset++;
    }
    if (offset < SERIAL_LINK_ALIGNMENT_LIMIT) {
        for (uint8_t index = 0; index < SERIAL_PACKET_SIZE; index++) {
            received_packet[index] = receive_dma[offset + index];
        }
        frame_ready = true;
    }
    transfer_active = false;
    IFS4bits.DMA6IF = 0;
}

/**
 * @brief Sends the next raw updater request byte.
 *
 * Feeds UART3 until the queued direct-mode request is empty, then releases the periodic service to
 * begin its receive turnaround.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _U3TXInterrupt(void) {
    IFS5bits.U3TXIF = 0;
    if (!direct_mode || !direct_transmit_active) {
        return;
    }
    if (direct_transmit_index < direct_transmit_length) {
        U3TXREG = direct_transmit[direct_transmit_index];
        direct_transmit_index++;
    } else {
        direct_transmit_active = false;
    }
}

/**
 * @brief Handles the first received UART3 byte.
 *
 * Retains bounded raw updater bytes in direct mode. During framed operation, disables further
 * receive interrupts and starts the 10000-cycle full-frame timeout.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _U3RXInterrupt(void) {
    if (direct_mode) {
        IFS5bits.U3RXIF = 0;
        if (U3STAbits.OERR != 0) {
            U3STAbits.OERR = 0;
            return;
        }
        if (U3STAbits.FERR != 0 || U3STAbits.PERR != 0) {
            U3STAbits.FERR = 0;
            U3STAbits.PERR = 0;
            return;
        }
        while (U3STAbits.URXDA != 0) {
            uint8_t received = U3RXREG;
            if (direct_receive_length < SERIAL_LINK_DIRECT_RECEIVE_CAPACITY) {
                direct_receive[direct_receive_length] = received;
                direct_receive_length++;
            }
        }
        return;
    }
    IEC5bits.U3RXIE = 0;
    IFS5bits.U3RXIF = 0;
    if (transfer_active && DMA6CONbits.CHEN != 0) {
        start_timer(SERIAL_LINK_RECEIVE_TIMEOUT_PERIOD, SERIAL_LINK_TIMER_RECEIVE_TIMEOUT);
    }
}

/**
 * @brief Handles attached-device link timing completion.
 *
 * Arms receive DMA after transmit turnaround or releases an exchange whose receive frame timed out.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _T6Interrupt(void) {
    if (direct_mode && timer_action == SERIAL_LINK_TIMER_DIRECT_SERVICE) {
        IFS2bits.T6IF = 0;
        service_direct_mode();
        return;
    }
    SerialLinkTimerAction action = timer_action;
    stop_timer();
    if (action == SERIAL_LINK_TIMER_START_RECEIVE && transfer_active) {
        start_receive();
    } else if (action == SERIAL_LINK_TIMER_RECEIVE_TIMEOUT) {
        DMA6CONbits.CHEN = 0;
        IEC4bits.DMA6IE = 0;
        IEC5bits.U3RXIE = 0;
        clear_uart();
        transfer_active = false;
    }
}

/**
 * @brief Handles UART3 receive errors.
 *
 * Clears overrun, framing, and parity state and drains any pending receive bytes.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _U3ErrInterrupt(void) {
    clear_uart();
    IFS5bits.U3EIF = 0;
}
