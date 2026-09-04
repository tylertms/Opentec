#include "platform/pedal_link.h"

#include <libpic30.h>
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
    PEDAL_FRAME_TYPE_AXIS_SAMPLE = 1,  /**< V3 axis-sample report type. */
    PEDAL_FRAME_TYPE_CONNECTION = 4,   /**< V3 connection report type. */
    PEDAL_FRAME_TYPE_CALIBRATION = 5,  /**< V3 calibration report type. */
    PEDAL_FRAME_TYPE_BRAKE_FORCE = 7,  /**< V3 brake-force report type. */
    PEDAL_FRAME_TYPE_SHARED_AXES = 8,  /**< V3 shared-axes report type. */
    PEDAL_ANALOG_SETTLE_DELAY_COUNT = 0x4ff, /**< Official analog fallback delay count. */
    PEDAL_ANALOG_SETTLE_DELAY_CYCLES =
        (PEDAL_ANALOG_SETTLE_DELAY_COUNT + 1UL) * 0x0c81UL * 2UL, /**< Equivalent delay cycles. */
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
 * @brief Retained byte ring for variable-length pedal transfer reception.
 *
 * The ring matches the official 256-byte continuous DMA receive window and keeps bytes until the
 * foreground transfer consumer advances its parser.
 */
static volatile uint8_t transfer_receive_ring[TRANSFER_FRAME_MAX_ENCODED_SIZE];

/**
 * @brief In-progress V4 pedal transfer frame.
 *
 * The parser retains at most the 124 bytes accepted by the official firmware.
 */
static volatile uint8_t transfer_buffer[TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE];

/**
 * @brief Shared pedal transmit DMA buffer.
 */
static volatile uint8_t transmitted_dma[TRANSFER_FRAME_MAX_ENCODED_SIZE];

/**
 * @brief Most recently received byte for byte-oriented pedal modes.
 */
static volatile uint8_t received_byte;

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
 * @brief Index of the oldest byte in the retained transfer receive ring.
 */
static volatile uint8_t transfer_receive_ring_head;

/**
 * @brief Number of retained bytes in the transfer receive ring.
 */
static volatile uint16_t transfer_receive_ring_count;

/**
 * @brief True while a pedal DMA transmission is active.
 */
static volatile bool transmit_active;

/**
 * @brief True while UART2 receive servicing is selected for the current pedal mode.
 *
 * Transmit completion uses this gate so a late DMA2 interrupt cannot re-enable reception during the
 * official reconnect hold or analog-input mode.
 */
static volatile bool receive_enabled;

/**
 * @brief True while fixed-size receive DMA is selected for the current pedal mode.
 */
static volatile bool receive_dma_enabled;

/**
 * @brief True while fixed-size frame reception is seeking a closing delimiter.
 */
static volatile bool resynchronizing;

/**
 * @brief True while variable-length transfer reception is selected.
 */
static volatile bool transfer_receiving;

/**
 * @brief True when the parser has retained one complete transfer frame for the foreground.
 */
static volatile bool transfer_frame_ready;

/**
 * @brief True when UART bytes are being assembled as transfer frames.
 *
 * The transmit-DMA completion path uses this state to select the official V4 receive-generation
 * reset instead of merely restoring the UART interrupt.
 */
static volatile bool transfer_receive_enabled;

static void receive_transfer_byte(uint8_t value);

/**
 * @brief Clears all retained V4 pedal receive state.
 *
 * Resets the byte ring and parser so bytes from an earlier protocol generation cannot complete a
 * frame after a reconnect or before a new V4 transmit.
 */
static void clear_transfer_receive_state(void) {
    transfer_receive_ring_head = 0;
    transfer_receive_ring_count = 0;
    transfer_buffer_length = 0;
    transfer_receiving = false;
    transfer_frame_ready = false;
}

/**
 * @brief Empties the UART2 receive FIFO.
 *
 * Discards every pending byte and clears an overrun condition so reception can resume.
 */
static void clear_receive_fifo(void) {
    while (U2STAbits.URXDA != 0) {
        U2RXREG;
    }
    U2STAbits.OERR = 0;
}

/**
 * @brief Computes the fixed pedal-frame checksum.
 *
 * Applies the reflected CRC-8 polynomial used by the official receive-DMA validator to the report
 * type and eight payload bytes.
 *
 * @param[in] input Report type followed by the payload bytes.
 * @return The calculated checksum byte.
 */
static uint8_t calculate_frame_checksum(const volatile uint8_t *input) {
    uint8_t checksum = UINT8_MAX;
    for (uint8_t index = 0; index < PEDAL_FRAME_PAYLOAD_SIZE + 1; index++) {
        checksum ^= input[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            checksum = (checksum & 1u) != 0
                           ? (uint8_t)((checksum >> 1) ^ 0x8cu)
                           : (uint8_t)(checksum >> 1);
        }
    }
    return checksum;
}

/**
 * @brief Reports whether a decoded V3 report type is accepted by receive DMA.
 *
 * Matches the five report cases reached by the official DMA ISR.
 *
 * @param[in] type Decoded report type.
 * @return True for an official report type; otherwise false.
 */
static bool is_supported_frame_type(uint8_t type) {
    return type == PEDAL_FRAME_TYPE_AXIS_SAMPLE || type == PEDAL_FRAME_TYPE_CONNECTION ||
           type == PEDAL_FRAME_TYPE_CALIBRATION || type == PEDAL_FRAME_TYPE_BRAKE_FORCE ||
           type == PEDAL_FRAME_TYPE_SHARED_AXES;
}

/**
 * @brief Reports whether a receive-DMA frame has the official boundary markers.
 *
 * @return True when the frame starts and ends with the protocol delimiters; otherwise false.
 */
static bool has_valid_frame_boundary(void) {
    return received_dma[0] == PEDAL_FRAME_START &&
           received_dma[PEDAL_FRAME_SIZE - 1] == PEDAL_FRAME_END;
}

/**
 * @brief Validates the boundary, checksum, and report type of a receive-DMA frame.
 *
 * Separates boundary failure, which enters UART delimiter recovery, from complete-frame failures,
 * which simply rearm receive DMA.
 *
 * @return True when the fixed frame is accepted by the official DMA gate.
 */
static bool is_valid_received_frame(void) {
    return has_valid_frame_boundary() &&
           received_dma[PEDAL_FRAME_SIZE - 2] == calculate_frame_checksum(received_dma + 1) &&
           is_supported_frame_type(received_dma[1]);
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
    clear_transfer_receive_state();
    transmit_active = false;
    receive_enabled = true;
    receive_dma_enabled = false;
    resynchronizing = false;
    transfer_receive_enabled = false;
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
    clear_transfer_receive_state();
    resynchronizing = false;
    transfer_receive_enabled = false;
    receive_enabled = true;
    receive_dma_enabled = false;
    IFS1bits.U2RXIF = 0;
    IEC1bits.U2RXIE = 1;
}

/**
 * @brief Selects direct analog pedal input.
 *
 * Stops UART and DMA reception, selects the three pedal analog inputs, makes the two detection
 * pins inputs, drives both detection latches low, waits through the official analog-settle delay,
 * and clears stale serial results.
 */
void platform_pedal_link_begin_analog(void) {
    IEC0bits.DMA1IE = 0;
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2MODEbits.UARTEN = 0;
    TRISFbits.TRISF1 = 1;
    TRISFbits.TRISF0 = 1;
    LATFbits.LATF1 = 0;
    PORTFbits.RF0 = 0;
    ANSELBbits.ANSB13 = 1;
    ANSELBbits.ANSB14 = 1;
    ANSELBbits.ANSB15 = 1;
    __delay32(PEDAL_ANALOG_SETTLE_DELAY_CYCLES);
    byte_ready = false;
    frame_ready = false;
    clear_transfer_receive_state();
    resynchronizing = false;
    transfer_receive_enabled = false;
    receive_enabled = false;
    receive_dma_enabled = false;
}

/**
 * @brief Stops pedal UART and DMA reception.
 *
 * Disables receive interrupts and DMA, turns off UART2, drains stale input, and clears all
 * published receive state for the official reconnect hold.
 */
void platform_pedal_link_stop_receive(void) {
    IEC0bits.DMA1IE = 0;
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2MODEbits.UARTEN = 0;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    clear_transfer_receive_state();
    resynchronizing = false;
    transfer_receive_enabled = false;
    receive_enabled = false;
    receive_dma_enabled = false;
    IFS0bits.DMA1IF = 0;
    IFS1bits.U2RXIF = 0;
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
    clear_transfer_receive_state();
    resynchronizing = false;
    transfer_receive_enabled = false;
    receive_enabled = true;
    receive_dma_enabled = true;
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
 * enables byte-oriented collection of escaped transfer bytes in the retained receive ring.
 */
void platform_pedal_link_begin_transfer_receive(void) {
    IEC0bits.DMA1IE = 0;
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2BRG = PEDAL_MODERN_BAUD_PERIOD;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    clear_transfer_receive_state();
    resynchronizing = false;
    transfer_receive_enabled = true;
    receive_enabled = true;
    receive_dma_enabled = false;
    IFS1bits.U2RXIF = 0;
    IEC1bits.U2RXIE = 1;
}

/**
 * @brief Starts one pedal DMA transmission.
 *
 * Claims the transmitter while its completion interrupt is masked, configures the requested byte
 * count, starts the channel, and forces its first UART2 request. Transfer-mode UART reception stays
 * enabled so overlapping V4 response bytes remain in the receive ring.
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
    if (!transfer_receive_enabled) {
        IEC1bits.U2RXIE = 0;
    }
    DMA2CNT = length - 1;
    IFS1bits.DMA2IF = 0;
    transmit_active = true;
    DMA2CONbits.CHEN = 1;
    DMA2REQbits.FORCE = 1;
    IEC1bits.DMA2IE = 1;
    return true;
}

/**
 * @brief Resets the V4 receive generation around a transfer.
 *
 * Stops receive DMA and its UART interrupt before draining stale input, clears the DMA status,
 * reloads the official 256-byte receive window, clears the retained ring and parser, and restores
 * the selected UART receive route.
 */
static void reset_transfer_receive_generation(void) {
    DMA1CONbits.CHEN = 0;
    IEC1bits.U2RXIE = 0;
    clear_receive_fifo();
    IFS0bits.DMA1IF = 0;
    DMA1CNT = TRANSFER_FRAME_MAX_ENCODED_SIZE - 1;
    clear_transfer_receive_state();
    IEC1bits.U2RXIE = receive_enabled;
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
    if (transfer_receive_enabled) {
        reset_transfer_receive_generation();
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
    IEC1bits.U2RXIE = receive_enabled;
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
    IEC0bits.DMA1IE = receive_dma_enabled;
    return ready;
}

/**
 * @brief Takes one complete encoded V4 transfer frame.
 *
 * Masks UART2 receive events while draining retained bytes, parsing one complete frame, and
 * copying it when it fits the caller's capacity. An undersized destination leaves the pending
 * frame available for a later call.
 *
 * @param[out] data Destination for the encoded frame.
 * @param[in] capacity Available destination bytes.
 * @return Encoded frame length, or zero when no complete frame fits.
 */
uint16_t platform_pedal_link_take_transfer(uint8_t *data, uint16_t capacity) {
    IEC1bits.U2RXIE = 0;
    while (transfer_receive_ring_count != 0 && !transfer_frame_ready) {
        uint8_t value = transfer_receive_ring[transfer_receive_ring_head];
        transfer_receive_ring_head++;
        transfer_receive_ring_count--;
        receive_transfer_byte(value);
    }
    uint16_t pending_length = transfer_frame_ready ? transfer_buffer_length : 0;
    uint16_t length = data != NULL && pending_length <= capacity ? pending_length : 0;
    if (length != 0) {
        for (uint16_t index = 0; index < length; index++) {
            data[index] = transfer_buffer[index];
        }
        transfer_frame_ready = false;
        transfer_buffer_length = 0;
    }
    IEC1bits.U2RXIE = receive_enabled;
    return length;
}

/**
 * @brief Accumulates one encoded V4 transfer byte.
 *
 * Retains bytes in the official-size receive ring until the foreground parser consumes them. Starts
 * collection at the opening marker while idle, appends nested markers, abandons frames after 124
 * bytes, and retains one complete frame until the caller accepts it.
 *
 * @param[in] value Received encoded byte.
 */
static void receive_transfer_byte(uint8_t value) {
    if (transfer_frame_ready) {
        return;
    }
    if (!transfer_receiving) {
        if (value == TRANSFER_FRAME_START) {
            transfer_buffer[0] = value;
            transfer_buffer_length = 1;
            transfer_receiving = true;
        }
        return;
    }
    if (transfer_buffer_length >= TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE) {
        transfer_buffer_length = 0;
        transfer_receiving = false;
        return;
    }

    transfer_buffer[transfer_buffer_length++] = value;
    if (value != TRANSFER_FRAME_END) {
        return;
    }

    transfer_frame_ready = true;
    transfer_receiving = false;
}

/**
 * @brief Retains one clean V4 receive byte.
 *
 * Advances the oldest byte when the official-size ring is full, preserving the newest serial
 * generation for the foreground parser.
 *
 * @param[in] value Clean encoded transfer byte to retain.
 */
static void retain_transfer_receive_byte(uint8_t value) {
    if (transfer_receive_ring_count == sizeof(transfer_receive_ring)) {
        transfer_receive_ring_head++;
        transfer_receive_ring_count--;
    }
    uint8_t slot = (uint8_t)(transfer_receive_ring_head + transfer_receive_ring_count);
    transfer_receive_ring[slot] = value;
    transfer_receive_ring_count++;
}

/**
 * @brief Completes UART delimiter recovery after one receive event.
 *
 * Uses only the final value drained from a recovery FIFO pass. The official receiver rearms DMA
 * only when that final value is the closing delimiter.
 *
 * @param[in] was_resynchronizing True when the receive event started in delimiter recovery.
 * @param[in] byte_received True when the receive event drained at least one value.
 * @param[in] value Final value drained from the receive FIFO.
 */
static void complete_receive_recovery(bool was_resynchronizing, bool byte_received,
                                      uint8_t value) {
    if (!was_resynchronizing || !byte_received || value != PEDAL_FRAME_END) {
        return;
    }
    resynchronizing = false;
    DMA1CONbits.CHEN = 1;
}

/**
 * @brief Processes one UART2 receive value.
 *
 * Assembles transfer bytes, ignores stray UART values while fixed-size DMA owns reception, and
 * publishes byte-oriented responses without filtering UART status flags.
 *
 * @param[in] value Received UART2 value.
 */
static void process_received_byte(uint8_t value) {
    if (transfer_receive_enabled) {
        retain_transfer_receive_byte(value);
        return;
    }
    if (resynchronizing || receive_dma_enabled) {
        return;
    }
    received_byte = value;
    byte_ready = true;
}

/**
 * @brief Services UART2 pedal receive events.
 *
 * Drains all available bytes, assembles V4 transfer frames, publishes byte-oriented responses, or
 * rearms V3 DMA from the final value after delimiter recovery. UART framing status is not used to
 * reject a value; normal-path UART overruns are cleared before return.
 */
void __attribute__((interrupt, no_auto_psv)) _U2RXInterrupt(void) {
    if (!receive_enabled) {
        clear_receive_fifo();
        IFS1bits.U2RXIF = 0;
        return;
    }
    bool was_resynchronizing = resynchronizing;
    bool byte_received = false;
    uint8_t final_value = 0;
    while (U2STAbits.URXDA != 0) {
        final_value = (uint8_t)U2RXREG;
        byte_received = true;
        if (!was_resynchronizing) {
            process_received_byte(final_value);
        }
    }
    complete_receive_recovery(was_resynchronizing, byte_received, final_value);
    if (!was_resynchronizing && U2STAbits.OERR) {
        U2STAbits.OERR = 0;
    }
    IFS1bits.U2RXIF = 0;
}

/**
 * @brief Services fixed-size pedal receive completion.
 *
 * Publishes a twelve-byte frame with valid boundaries, checksum, and recognized report type, then
 * immediately rearms DMA. A malformed-boundary frame switches reception to UART delimiter recovery
 * while a bad checksum or unknown report type simply rearms the next DMA frame.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA1Interrupt(void) {
    if (!receive_dma_enabled) {
        IFS0bits.DMA1IF = 0;
        return;
    }
    DMA1CNT = PEDAL_FRAME_SIZE - 1;
    if (!has_valid_frame_boundary()) {
        resynchronizing = true;
        IEC1bits.U2RXIE = 1;
    } else {
        if (is_valid_received_frame()) {
            for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
                received_frame[index] = received_dma[index];
            }
            frame_ready = true;
        }
        DMA1CONbits.CHEN = 1;
    }
    IFS0bits.DMA1IF = 0;
}

#ifdef OPENTEC_SIMULATOR_TEST
/**
 * @brief Loads a receive-DMA frame for platform tests.
 *
 * @param[in] frame Twelve bytes to expose through the simulated receive DMA buffer.
 * @param[in] uart_error True to set the simulated UART overrun status while the frame is checked.
 */
void platform_pedal_link_test_set_receive_dma(const uint8_t frame[PEDAL_FRAME_SIZE],
                                              bool uart_error) {
    for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
        received_dma[index] = frame[index];
    }
    if (uart_error) {
        U2STAbits.OERR = 1;
    }
}

/**
 * @brief Feeds one UART2 value through receive and recovery logic.
 *
 * @param[in] value Value to process.
 * @param[in] uart_error True to set the simulated UART framing status while the value is processed.
 */
void platform_pedal_link_test_receive_byte(uint8_t value, bool uart_error) {
    if (receive_enabled) {
        bool was_resynchronizing = resynchronizing;
        if (uart_error) {
            U2STAbits.FERR = 1;
        }
        process_received_byte(value);
        complete_receive_recovery(was_resynchronizing, true, value);
        U2STAbits.FERR = 0;
    }
}

/**
 * @brief Feeds a simulated UART receive FIFO burst.
 *
 * Recovery examines only the final value in the burst, matching the UART ISR's FIFO-drain order.
 *
 * @param[in] values Values exposed in FIFO order.
 * @param[in] length Number of values in the burst.
 */
void platform_pedal_link_test_receive_burst(const uint8_t *values, uint8_t length) {
    if (!receive_enabled || values == NULL || length == 0) {
        return;
    }
    bool was_resynchronizing = resynchronizing;
    uint8_t final_value = 0;
    for (uint8_t index = 0; index < length; index++) {
        final_value = values[index];
        if (!was_resynchronizing) {
            process_received_byte(final_value);
        }
    }
    complete_receive_recovery(was_resynchronizing, true, final_value);
}
#endif

/**
 * @brief Services pedal transmit completion.
 *
 * Releases the shared transmitter, clears the DMA completion flag, and restores the selected receive
 * generation. Transfer mode also resets the UART FIFO, DMA state, parser, and retained ring before
 * receive servicing is restored.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA2Interrupt(void) {
    IFS1bits.DMA2IF = 0;
    transmit_active = false;
    if (transfer_receive_enabled) {
        reset_transfer_receive_generation();
    } else {
        IEC1bits.U2RXIE = receive_enabled;
    }
}
