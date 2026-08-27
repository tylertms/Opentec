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
};

static volatile uint8_t transmit_dma[WHEEL_LINK_TRANSMIT_SIZE];
static volatile uint8_t receive_dma[WHEEL_LINK_RECEIVE_SIZE];
static volatile uint8_t received_frame[WHEEL_TRANSPORT_FRAME_SIZE];
static volatile bool transfer_active;
static volatile bool frame_ready;

static void clear_uart(void) {
    U3STAbits.OERR = 0;
    while (U3STAbits.URXDA != 0) {
        (void)U3RXREG;
    }
}

/**
 * Configures UART3 for the normal attached-wheel link.
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
 * Configures the byte-oriented, one-shot UART3 DMA channels.
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
 * Enables wheel-link DMA and UART error interrupts.
 *
 * DMA5 uses priority 5 and DMA6 uses priority 4.
 */
static void configure_interrupts(void) {
    IPC15bits.DMA5IP = WHEEL_LINK_TRANSMIT_PRIORITY;
    IPC17bits.DMA6IP = WHEEL_LINK_RECEIVE_PRIORITY;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    IEC3bits.DMA5IE = 1;
    IEC4bits.DMA6IE = 1;
    IEC5bits.U3EIE = 1;
}

void platform_wheel_link_init(void) {
    transfer_active = false;
    frame_ready = false;
    TRISFbits.TRISF2 = 0;
    TRISFbits.TRISF8 = 1;
    CNPDFbits.CNPDF2 = 1;
    CNPDFbits.CNPDF8 = 1;
    configure_uart();
    configure_dma();
    configure_interrupts();
    clear_uart();
}

void platform_wheel_link_reset(void) {
    IEC3bits.DMA5IE = 0;
    IEC4bits.DMA6IE = 0;
    DMA5CONbits.CHEN = 0;
    DMA6CONbits.CHEN = 0;
    clear_uart();
    transfer_active = false;
    frame_ready = false;
    IFS3bits.DMA5IF = 0;
    IFS4bits.DMA6IF = 0;
    IEC3bits.DMA5IE = 1;
    IEC4bits.DMA6IE = 1;
}

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

void __attribute__((interrupt, no_auto_psv)) _DMA5Interrupt(void) {
    DMA5CONbits.CHEN = 0;
    DMA6CONbits.CHEN = 1;
    IFS3bits.DMA5IF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _DMA6Interrupt(void) {
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
    DMA6CONbits.CHEN = 0;
    transfer_active = false;
    IFS4bits.DMA6IF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _U3ErrInterrupt(void) {
    clear_uart();
    IFS5bits.U3EIF = 0;
}
