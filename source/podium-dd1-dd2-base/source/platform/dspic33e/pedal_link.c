#include "platform/pedal_link.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xc.h>

/**
 * @brief Pedal UART and DMA configuration values.
 */
enum {
    PEDAL_LEGACY_BAUD_PERIOD = 0x39,   /**< UART2 baud-period value for discovery traffic. */
    PEDAL_MODERN_BAUD_PERIOD = 0x81,   /**< UART2 baud-period value for modern pedal traffic. */
    PEDAL_RECEIVE_DMA_REQUEST = 0x1e,  /**< DMA request number for UART2 reception. */
    PEDAL_TRANSMIT_DMA_REQUEST = 0x1f, /**< DMA request number for UART2 transmission. */
    PEDAL_INTERRUPT_PRIORITY = 4,      /**< Pedal UART and DMA interrupt priority. */
};

/**
 * @brief Pedal receive DMA buffer.
 */
static volatile uint8_t received_dma[PEDAL_FRAME_SIZE];

/**
 * @brief Most recently validated fixed-size pedal frame.
 */
static volatile uint8_t received_frame[PEDAL_FRAME_SIZE];

/**
 * @brief Two-entry queue of complete variable-length pedal transfer frames.
 */
static volatile uint8_t received_transfer[2][TRANSFER_FRAME_MAX_RECEIVED_SIZE];

/**
 * @brief In-progress variable-length pedal transfer frame.
 */
static volatile uint8_t transfer_buffer[TRANSFER_FRAME_MAX_RECEIVED_SIZE];

/**
 * @brief Shared pedal transmit DMA buffer.
 */
static volatile uint8_t transmitted_dma[TRANSFER_FRAME_MAX_ENCODED_SIZE];

/**
 * @brief Most recently received byte for byte-oriented pedal modes.
 */
static volatile uint8_t received_byte;

/**
 * @brief Encoded length of each queued variable-length transfer frame.
 */
static volatile uint16_t received_transfer_length[2];

/**
 * @brief Number of bytes in the in-progress transfer frame.
 */
static volatile uint16_t transfer_buffer_length;

/**
 * @brief True when a byte-oriented response is ready for the foreground.
 */
static volatile bool byte_ready;

/**
 * @brief True when a fixed-size pedal frame is ready for the foreground.
 */
static volatile bool frame_ready;

/**
 * @brief Queue index of the oldest variable-length transfer frame.
 */
static volatile uint8_t received_transfer_head;

/**
 * @brief Number of queued variable-length transfer frames.
 */
static volatile uint8_t received_transfer_count;

/**
 * @brief True while a pedal DMA transmission is active.
 */
static volatile bool transmit_active;

/**
 * @brief True while fixed-size frame reception is seeking a closing delimiter.
 */
static volatile bool resynchronizing;

/**
 * @brief True while variable-length transfer reception is selected.
 */
static volatile bool transfer_receiving;

/**
 * @brief True when UART bytes are being assembled as transfer frames.
 */
static volatile bool transfer_receive_enabled;

/**
 * @brief Clears all completed V4 pedal receive frames.
 *
 * Resets the double-buffer queue head and count without changing the in-progress receive buffer.
 */
static void clear_transfer_queue(void) {
    received_transfer_head = 0;
    received_transfer_count = 0;
}

/**
 * @brief Empties the UART2 receive FIFO.
 *
 * Discards every pending byte and clears an overrun condition so reception can resume.
 */
static void clear_receive_fifo(void) {
    while (U2STAbits.URXDA != 0) {
        (void)U2RXREG;
    }
    U2STAbits.OERR = 0;
}

/**
 * @brief Configures UART2 for pedal communication.
 *
 * Selects high-speed baud generation, the legacy period, transmitter operation, and idle-mode
 * suspension before enabling the peripheral.
 */
static void configure_uart(void) {
    U2MODEbits.UARTEN = 0;
    U2MODE = 0;
    U2STA = 0;
    U2MODEbits.USIDL = 1;
    U2MODEbits.BRGH = 1;
    U2BRG = PEDAL_LEGACY_BAUD_PERIOD;
    U2STAbits.UTXEN = 1;
    U2MODEbits.UARTEN = 1;
}

/**
 * @brief Configures the pedal receive DMA channel.
 *
 * Routes UART2 receive requests to one-shot byte transfers into a twelve-byte frame buffer.
 */
static void configure_receive_dma(void) {
    DMA1CON = 0;
    DMA1CONbits.SIZE = 1;
    DMA1CONbits.DIR = 0;
    DMA1CONbits.AMODE = 0;
    DMA1CONbits.MODE = 1;
    DMA1REQbits.IRQSEL = PEDAL_RECEIVE_DMA_REQUEST;
    DMA1PAD = (uint16_t)&U2RXREG;
    DMA1STAL = (uint16_t)received_dma;
    DMA1STAH = 0;
    DMA1CNT = PEDAL_FRAME_SIZE - 1;
}

/**
 * @brief Configures the pedal transmit DMA channel.
 *
 * Routes one-shot byte transfers from the shared transmit buffer to UART2.
 */
static void configure_transmit_dma(void) {
    DMA2CON = 0;
    DMA2CONbits.SIZE = 1;
    DMA2CONbits.DIR = 1;
    DMA2CONbits.AMODE = 0;
    DMA2CONbits.MODE = 1;
    DMA2REQbits.IRQSEL = PEDAL_TRANSMIT_DMA_REQUEST;
    DMA2PAD = (uint16_t)&U2TXREG;
    DMA2STAL = (uint16_t)transmitted_dma;
    DMA2STAH = 0;
}

/**
 * @brief Configures pedal transport interrupt routing.
 *
 * Assigns priority four to receive DMA, transmit DMA, and UART2 receive events. Receive DMA starts
 * disabled, while transmit completion and UART receive servicing start enabled.
 */
static void configure_interrupts(void) {
    IPC3bits.DMA1IP = PEDAL_INTERRUPT_PRIORITY;
    IPC6bits.DMA2IP = PEDAL_INTERRUPT_PRIORITY;
    IPC7bits.U2RXIP = PEDAL_INTERRUPT_PRIORITY;
    IFS0bits.DMA1IF = 0;
    IFS1bits.DMA2IF = 0;
    IFS1bits.U2RXIF = 0;
    IEC0bits.DMA1IE = 0;
    IEC1bits.DMA2IE = 1;
    IEC1bits.U2RXIE = 1;
}

/**
 * @brief Initializes the pedal communication port.
 *
 * Clears all published transport state, configures UART2 and both DMA channels, drains stale
 * receive data, and then enables their interrupt routes.
 */
void platform_pedal_link_init(void) {
    IEC1bits.U2TXIE = 0;
    byte_ready = false;
    frame_ready = false;
    clear_transfer_queue();
    transmit_active = false;
    resynchronizing = false;
    transfer_receiving = false;
    transfer_receive_enabled = false;
    transfer_buffer_length = 0;
    configure_uart();
    configure_receive_dma();
    configure_transmit_dma();
    clear_receive_fifo();
    configure_interrupts();
}

/**
 * @brief Selects byte-oriented pedal discovery.
 *
 * Stops framed reception, selects digital UART pins and the legacy baud period, clears stale
 * results, and enables UART2 receive events.
 */
void platform_pedal_link_begin_discovery(void) {
    IEC0bits.DMA1IE = 0;
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    ANSELBbits.ANSB13 = 0;
    ANSELBbits.ANSB14 = 0;
    U2MODEbits.UARTEN = 1;
    U2STAbits.UTXEN = 1;
    U2BRG = PEDAL_LEGACY_BAUD_PERIOD;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    clear_transfer_queue();
    resynchronizing = false;
    transfer_receiving = false;
    transfer_receive_enabled = false;
    IFS1bits.U2RXIF = 0;
    IEC1bits.U2RXIE = 1;
}

/**
 * @brief Selects direct analog pedal input.
 *
 * Stops UART and DMA reception, selects the three pedal analog inputs, makes the two detection
 * pins inputs, and clears stale serial results.
 */
void platform_pedal_link_begin_analog(void) {
    IEC0bits.DMA1IE = 0;
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2MODEbits.UARTEN = 0;
    ANSELBbits.ANSB13 = 1;
    ANSELBbits.ANSB14 = 1;
    ANSELBbits.ANSB15 = 1;
    TRISFbits.TRISF0 = 1;
    TRISFbits.TRISF1 = 1;
    byte_ready = false;
    frame_ready = false;
    clear_transfer_queue();
    resynchronizing = false;
    transfer_receiving = false;
    transfer_receive_enabled = false;
}

/**
 * @brief Selects fixed-size V3 pedal reception.
 *
 * Selects the modern baud period, drains stale bytes, clears prior results, and arms receive DMA
 * for one twelve-byte frame while retaining UART delimiter recovery.
 */
void platform_pedal_link_begin_framed_receive(void) {
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2BRG = PEDAL_MODERN_BAUD_PERIOD;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    clear_transfer_queue();
    resynchronizing = false;
    transfer_receiving = false;
    transfer_receive_enabled = false;
    DMA1CNT = PEDAL_FRAME_SIZE - 1;
    IFS0bits.DMA1IF = 0;
    IEC0bits.DMA1IE = 1;
    DMA1CONbits.CHEN = 1;
    IEC1bits.U2RXIE = 1;
}

/**
 * @brief Selects variable-length V4 transfer reception.
 *
 * Stops fixed-size DMA reception, selects the modern baud period, clears prior results, and
 * enables byte-oriented collection of escaped transfer frames.
 */
void platform_pedal_link_begin_transfer_receive(void) {
    IEC0bits.DMA1IE = 0;
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2BRG = PEDAL_MODERN_BAUD_PERIOD;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    clear_transfer_queue();
    resynchronizing = false;
    transfer_receiving = false;
    transfer_receive_enabled = true;
    transfer_buffer_length = 0;
    IFS1bits.U2RXIF = 0;
    IEC1bits.U2RXIE = 1;
}

/**
 * @brief Starts one pedal DMA transmission.
 *
 * Claims the transmitter while its completion interrupt is masked, configures the requested byte
 * count, starts the channel, and forces its first UART2 request.
 *
 * @param[in] length Number of bytes already prepared in the transmit buffer.
 * @return True when the transmission started; false when another transmission owns the channel.
 */
static bool begin_send(uint16_t length) {
    IEC1bits.DMA2IE = 0;
    if (transmit_active) {
        IEC1bits.DMA2IE = 1;
        return false;
    }
    DMA2CONbits.CHEN = 0;
    IEC1bits.U2RXIE = 0;
    DMA2CNT = length - 1;
    IFS1bits.DMA2IF = 0;
    transmit_active = true;
    DMA2CONbits.CHEN = 1;
    DMA2REQbits.FORCE = 1;
    IEC1bits.DMA2IE = 1;
    return true;
}

/**
 * @brief Sends one pedal discovery or legacy command byte.
 *
 * Copies the command into the DMA source and starts a one-byte transmission when the channel is
 * idle.
 *
 * @param[in] value Command byte to send.
 * @return True when the command started; false while another transmission is active.
 */
bool platform_pedal_link_send_byte(uint8_t value) {
    if (transmit_active) {
        return false;
    }
    transmitted_dma[0] = value;
    return begin_send(1);
}

/**
 * @brief Sends one fixed-size V3 pedal frame.
 *
 * Copies all twelve encoded bytes into the DMA source and starts transmission when the channel is
 * idle.
 *
 * @param[in] frame Encoded pedal frame to send.
 * @return True when the frame started; false while another transmission is active.
 */
bool platform_pedal_link_send_frame(const uint8_t frame[PEDAL_FRAME_SIZE]) {
    if (transmit_active) {
        return false;
    }
    for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
        transmitted_dma[index] = frame[index];
    }
    return begin_send(PEDAL_FRAME_SIZE);
}

/**
 * @brief Sends one encoded V4 transfer frame.
 *
 * Rejects null, empty, oversized, or concurrent requests before copying the encoded bytes and
 * starting DMA transmission.
 *
 * @param[in] data Encoded transfer frame to send.
 * @param[in] length Encoded byte count.
 * @return True when the frame started; false when the request is invalid or the channel is busy.
 */
bool platform_pedal_link_send_transfer(const uint8_t *data, uint16_t length) {
    if (data == NULL || length == 0 || length > TRANSFER_FRAME_MAX_ENCODED_SIZE ||
        transmit_active) {
        return false;
    }
    for (uint16_t index = 0; index < length; index++) {
        transmitted_dma[index] = data[index];
    }
    return begin_send(length);
}

/**
 * @brief Reports pedal transmitter ownership.
 *
 * Returns the state cleared by the transmit DMA completion event.
 *
 * @return True while a pedal transmission is active; otherwise false.
 */
bool platform_pedal_link_transmit_busy(void) { return transmit_active; }

/**
 * @brief Takes the newest byte-oriented pedal response.
 *
 * Masks UART2 receive events while copying and consuming the pending discovery or legacy byte.
 *
 * @param[out] value Destination for the received byte.
 * @return True when a byte was consumed; otherwise false.
 */
bool platform_pedal_link_take_byte(uint8_t *value) {
    IEC1bits.U2RXIE = 0;
    bool ready = byte_ready;
    if (ready) {
        *value = received_byte;
        byte_ready = false;
    }
    IEC1bits.U2RXIE = 1;
    return ready;
}

/**
 * @brief Takes the newest boundary-aligned V3 pedal frame.
 *
 * Masks receive DMA completion while copying and consuming the pending twelve-byte frame.
 *
 * @param[out] frame Destination for the received frame.
 * @return True when a frame was consumed; otherwise false.
 */
bool platform_pedal_link_take_frame(uint8_t frame[PEDAL_FRAME_SIZE]) {
    IEC0bits.DMA1IE = 0;
    bool ready = frame_ready;
    if (ready) {
        for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
            frame[index] = received_frame[index];
        }
        frame_ready = false;
    }
    IEC0bits.DMA1IE = 1;
    return ready;
}

/**
 * @brief Takes one complete encoded V4 transfer frame.
 *
 * Masks UART2 receive events while copying and consuming a frame that fits the caller's capacity.
 * An undersized destination leaves the pending frame available for a later call.
 *
 * @param[out] data Destination for the encoded frame.
 * @param[in] capacity Available destination bytes.
 * @return Encoded frame length, or zero when no complete frame fits.
 */
uint16_t platform_pedal_link_take_transfer(uint8_t *data, uint16_t capacity) {
    IEC1bits.U2RXIE = 0;
    uint16_t pending_length =
        received_transfer_count != 0 ? received_transfer_length[received_transfer_head] : 0;
    uint16_t length = pending_length <= capacity ? pending_length : 0;
    if (length != 0) {
        for (uint16_t index = 0; index < length; index++) {
            data[index] = received_transfer[received_transfer_head][index];
        }
        received_transfer_head ^= 1u;
        received_transfer_count--;
    }
    IEC1bits.U2RXIE = 1;
    return length;
}

/**
 * @brief Accumulates one encoded V4 transfer byte.
 *
 * Starts or restarts collection at the opening marker, abandons oversized partial frames, and
 * appends a complete frame to the two-entry receive queue at the closing marker. A completed frame
 * is dropped when both queue entries remain unread.
 *
 * @param[in] value Received encoded byte.
 */
static void receive_transfer_byte(uint8_t value) {
    if (value == TRANSFER_FRAME_START) {
        transfer_buffer[0] = value;
        transfer_buffer_length = 1;
        transfer_receiving = true;
        return;
    }
    if (!transfer_receiving) {
        return;
    }
    if (transfer_buffer_length >= TRANSFER_FRAME_MAX_RECEIVED_SIZE) {
        transfer_buffer_length = 0;
        transfer_receiving = false;
        return;
    }

    transfer_buffer[transfer_buffer_length++] = value;
    if (value != TRANSFER_FRAME_END) {
        return;
    }

    if (received_transfer_count < 2) {
        uint8_t slot = (uint8_t)((received_transfer_head + received_transfer_count) & 1u);
        for (uint16_t index = 0; index < transfer_buffer_length; index++) {
            received_transfer[slot][index] = transfer_buffer[index];
        }
        received_transfer_length[slot] = transfer_buffer_length;
        received_transfer_count++;
    }
    transfer_buffer_length = 0;
    transfer_receiving = false;
}

/**
 * @brief Services UART2 pedal receive events.
 *
 * Drains all available bytes, assembles V4 transfer frames, publishes the newest discovery byte,
 * or rearms V3 DMA after a closing delimiter. UART overruns are cleared before return.
 */
void __attribute__((interrupt, no_auto_psv)) _U2RXInterrupt(void) {
    uint8_t last = 0;
    bool received = false;
    while (U2STAbits.URXDA != 0) {
        bool error = U2STAbits.FERR != 0 || U2STAbits.OERR != 0;
        last = (uint8_t)U2RXREG;
        if (error) {
            continue;
        }
        received = true;
        if (transfer_receive_enabled) {
            receive_transfer_byte(last);
        }
    }
    if (!transfer_receive_enabled && resynchronizing) {
        if (received && last == PEDAL_FRAME_END) {
            resynchronizing = false;
            DMA1CONbits.CHEN = 1;
        }
    } else if (!transfer_receive_enabled && received) {
        received_byte = last;
        byte_ready = true;
    }
    if (U2STAbits.OERR) {
        U2STAbits.OERR = 0;
    }
    IFS1bits.U2RXIF = 0;
}

/**
 * @brief Services fixed-size pedal receive completion.
 *
 * Publishes a twelve-byte frame with valid boundary markers and immediately rearms DMA. A malformed
 * boundary switches reception to UART delimiter recovery before the next DMA frame is accepted.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA1Interrupt(void) {
    DMA1CNT = PEDAL_FRAME_SIZE - 1;
    if (received_dma[0] == PEDAL_FRAME_START &&
        received_dma[PEDAL_FRAME_SIZE - 1] == PEDAL_FRAME_END) {
        for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
            received_frame[index] = received_dma[index];
        }
        frame_ready = true;
        DMA1CONbits.CHEN = 1;
    } else {
        resynchronizing = true;
        IEC1bits.U2RXIE = 1;
    }
    IFS0bits.DMA1IF = 0;
}

/**
 * @brief Services pedal transmit completion.
 *
 * Releases the shared transmitter, clears the DMA request, and restores UART2 receive servicing.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA2Interrupt(void) {
    transmit_active = false;
    IFS1bits.DMA2IF = 0;
    IEC1bits.U2RXIE = 1;
}
