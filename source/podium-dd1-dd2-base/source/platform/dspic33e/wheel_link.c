#include "platform/wheel_link.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

enum {
    WHEEL_LINK_TRANSMIT_SIZE = 72,
    WHEEL_LINK_RECEIVE_SIZE = 68,
    WHEEL_LINK_FRAME_OFFSET = 4,
    WHEEL_LINK_ALIGNMENT_LIMIT = 5,
    WHEEL_LINK_PADDING = 0xf0,
    WHEEL_LINK_BAUD_PERIOD = 2,
    WHEEL_LINK_TRANSMIT_DMA_REQUEST = 0x53,
    WHEEL_LINK_RECEIVE_DMA_REQUEST = 0x52,
    WHEEL_LINK_TRANSMIT_PRIORITY = 5,
    WHEEL_LINK_RECEIVE_PRIORITY = 4,
    WHEEL_LINK_TIMER_PRIORITY = 6,
    WHEEL_LINK_TRANSMIT_GUARD_PERIOD = 0x4b0,
    WHEEL_LINK_RECEIVE_TIMEOUT_PERIOD = 10000,
};

typedef enum {
    WHEEL_LINK_TIMER_IDLE,
    WHEEL_LINK_TIMER_RECEIVE_TIMEOUT,
    WHEEL_LINK_TIMER_START_RECEIVE,
} WheelLinkTimerAction;

static volatile uint8_t transmit_dma[WHEEL_LINK_TRANSMIT_SIZE];
static volatile uint8_t receive_dma[WHEEL_LINK_RECEIVE_SIZE];
static volatile uint8_t received_frame[WHEEL_TRANSPORT_FRAME_SIZE];
static volatile bool transfer_active;
static volatile bool frame_ready;
static volatile WheelLinkTimerAction timer_action;

/**
 * @brief Clears UART3 receive errors and pending bytes.
 *
 * Clears overrun, framing, and parity state before draining the receive FIFO.
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
 */
static void configure_uart(void) {
    U3MODEbits.UARTEN = 0;
    U3MODE = 0;
    U3STA = 0;
    U3MODEbits.BRGH = 1;
    U3MODEbits.URXINV = 1;
    U3BRG = WHEEL_LINK_BAUD_PERIOD;
    U3STAbits.UTXINV = 1;
    U3MODEbits.UARTEN = 1;
    U3STAbits.UTXEN = 1;
}

/**
 * @brief Configures the byte-oriented, one-shot UART3 DMA channels.
 *
 * DMA5 sends 72 bytes with request 0x53. DMA6 receives 68 bytes with request 0x52.
 */
static void configure_dma(void) {
    DMA5CON = 0;
    DMA5CONbits.SIZE = 1;
    DMA5CONbits.DIR = 1;
    DMA5CONbits.AMODE = 0;
    DMA5CONbits.MODE = 1;
    DMA5REQbits.IRQSEL = WHEEL_LINK_TRANSMIT_DMA_REQUEST;
    DMA5PAD = (uint16_t)&U3TXREG;
    DMA5STAL = (uint16_t)transmit_dma;
    DMA5STAH = 0;
    DMA5CNT = WHEEL_LINK_TRANSMIT_SIZE - 1;

    DMA6CON = 0;
    DMA6CONbits.SIZE = 1;
    DMA6CONbits.DIR = 0;
    DMA6CONbits.AMODE = 0;
    DMA6CONbits.MODE = 1;
    DMA6REQbits.IRQSEL = WHEEL_LINK_RECEIVE_DMA_REQUEST;
    DMA6PAD = (uint16_t)&U3RXREG;
    DMA6STAL = (uint16_t)receive_dma;
    DMA6STAH = 0;
    DMA6CNT = WHEEL_LINK_RECEIVE_SIZE - 1;
}

/**
 * @brief Configures attached-device link interrupts.
 *
 * DMA5 uses priority 5, DMA6 uses priority 4, and Timer 6 uses priority 6.
 */
static void configure_interrupts(void) {
    IPC15bits.DMA5IP = WHEEL_LINK_TRANSMIT_PRIORITY;
    IPC17bits.DMA6IP = WHEEL_LINK_RECEIVE_PRIORITY;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    IFS2bits.T6IF = 0;
    IFS5bits.U3EIF = 0;
    IFS5bits.U3RXIF = 0;
    IEC3bits.DMA5IE = 1;
    IEC4bits.DMA6IE = 1;
    IPC11bits.T6IP = WHEEL_LINK_TIMER_PRIORITY;
    IEC2bits.T6IE = 1;
    IEC5bits.U3EIE = 1;
}

/**
 * @brief Configures Timer 6 for attached-device link timing.
 *
 * Selects the internal clock with a 1:1 prescaler and leaves the timer stopped.
 */
static void configure_timer(void) {
    T6CON = 0;
    TMR6 = 0;
    PR6 = 0;
    timer_action = WHEEL_LINK_TIMER_IDLE;
}

/**
 * @brief Starts an attached-device link timing interval.
 *
 * Replaces the current Timer 6 interval with the supplied period and completion action.
 *
 * @param[in] period Timer 6 period in instruction cycles.
 * @param[in] action Action to perform when the period expires.
 */
static void start_timer(uint16_t period, WheelLinkTimerAction action) {
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
 */
static void stop_timer(void) {
    T6CONbits.TON = 0;
    TMR6 = 0;
    IFS2bits.T6IF = 0;
    timer_action = WHEEL_LINK_TIMER_IDLE;
}

/**
 * @brief Arms UART3 receive DMA for one 68-byte transfer.
 *
 * Clears the receive storage and UART state before enabling DMA6 and the first-byte interrupt.
 */
static void start_receive(void) {
    DMA6CONbits.CHEN = 0;
    IEC4bits.DMA6IE = 0;
    for (uint8_t index = 0; index < WHEEL_LINK_RECEIVE_SIZE; index++) {
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
 * @brief Initializes the attached-device UART3 exchange layer.
 *
 * Configures the physical pins, inverted high-speed UART, one-shot DMA channels, and Timer 6.
 */
void platform_wheel_link_init(void) {
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
 * Stops all active DMA and timer work, clears pending receive data, and releases the transaction.
 */
void platform_wheel_link_reset(void) {
    IEC3bits.DMA5IE = 0;
    IEC4bits.DMA6IE = 0;
    IEC5bits.U3RXIE = 0;
    DMA5CONbits.CHEN = 0;
    DMA6CONbits.CHEN = 0;
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
 * the transmit DMA. Only one exchange can be active at a time.
 *
 * @param[in] frame Transport frame to transmit.
 * @return true when the exchange starts; false when another exchange is active.
 */
bool platform_wheel_link_start(const uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]) {
    IEC3bits.DMA5IE = 0;
    if (transfer_active) {
        IEC3bits.DMA5IE = 1;
        return false;
    }
    for (uint8_t index = 0; index < WHEEL_LINK_TRANSMIT_SIZE; index++) {
        transmit_dma[index] = WHEEL_LINK_PADDING;
    }
    for (uint8_t index = 0; index < WHEEL_TRANSPORT_FRAME_SIZE; index++) {
        transmit_dma[WHEEL_LINK_FRAME_OFFSET + index] = frame[index];
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
 * Copies a newly aligned 64-byte response into caller storage and consumes its ready state.
 *
 * @param[out] frame Storage that receives the transport frame.
 * @return true when a frame was copied; false when no response is ready.
 */
bool platform_wheel_link_take_received(uint8_t frame[WHEEL_TRANSPORT_FRAME_SIZE]) {
    IEC4bits.DMA6IE = 0;
    bool ready = frame_ready;
    if (ready) {
        for (uint8_t index = 0; index < WHEEL_TRANSPORT_FRAME_SIZE; index++) {
            frame[index] = received_frame[index];
        }
        frame_ready = false;
    }
    IEC4bits.DMA6IE = 1;
    return ready;
}

/**
 * @brief Handles completion of the 72-byte UART3 transmit DMA.
 *
 * Stops DMA5 and schedules receive DMA after the 0x4b0-cycle turnaround guard.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA5Interrupt(void) {
    DMA5CONbits.CHEN = 0;
    IFS3bits.DMA5IF = 0;
    start_timer(WHEEL_LINK_TRANSMIT_GUARD_PERIOD, WHEEL_LINK_TIMER_START_RECEIVE);
}

/**
 * @brief Handles completion of the 68-byte UART3 receive DMA.
 *
 * Stops receive timing, aligns the first frame marker within the five accepted offsets, and makes
 * the response available to the service layer.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA6Interrupt(void) {
    DMA6CONbits.CHEN = 0;
    IEC4bits.DMA6IE = 0;
    stop_timer();
    uint8_t offset = 0;
    while (offset < WHEEL_LINK_ALIGNMENT_LIMIT &&
           receive_dma[offset] != WHEEL_TRANSPORT_FRAME_START) {
        offset++;
    }
    if (offset < WHEEL_LINK_ALIGNMENT_LIMIT) {
        for (uint8_t index = 0; index < WHEEL_TRANSPORT_FRAME_SIZE; index++) {
            received_frame[index] = receive_dma[offset + index];
        }
        frame_ready = true;
    }
    transfer_active = false;
    IFS4bits.DMA6IF = 0;
}

/**
 * @brief Handles the first received UART3 byte.
 *
 * Disables further UART3 receive interrupts and starts the 10000-cycle full-frame timeout.
 */
void __attribute__((interrupt, no_auto_psv)) _U3RXInterrupt(void) {
    IEC5bits.U3RXIE = 0;
    IFS5bits.U3RXIF = 0;
    if (transfer_active && DMA6CONbits.CHEN != 0) {
        start_timer(WHEEL_LINK_RECEIVE_TIMEOUT_PERIOD, WHEEL_LINK_TIMER_RECEIVE_TIMEOUT);
    }
}

/**
 * @brief Handles attached-device link timing completion.
 *
 * Arms receive DMA after transmit turnaround or releases an exchange whose receive frame timed out.
 */
void __attribute__((interrupt, no_auto_psv)) _T6Interrupt(void) {
    WheelLinkTimerAction action = timer_action;
    stop_timer();
    if (action == WHEEL_LINK_TIMER_START_RECEIVE && transfer_active) {
        start_receive();
    } else if (action == WHEEL_LINK_TIMER_RECEIVE_TIMEOUT) {
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
 */
void __attribute__((interrupt, no_auto_psv)) _U3ErrInterrupt(void) {
    clear_uart();
    IFS5bits.U3EIF = 0;
}
