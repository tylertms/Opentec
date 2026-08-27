#include "platform/pedal_link.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

enum {
    PEDAL_LEGACY_BAUD_PERIOD = 0x39,
    PEDAL_MODERN_BAUD_PERIOD = 0x81,
    PEDAL_RECEIVE_DMA_REQUEST = 0x1e,
    PEDAL_TRANSMIT_DMA_REQUEST = 0x1f,
    PEDAL_INTERRUPT_PRIORITY = 4,
};

static volatile uint8_t received_dma[PEDAL_FRAME_SIZE];
static volatile uint8_t received_frame[PEDAL_FRAME_SIZE];
static volatile uint8_t transmitted_dma[PEDAL_FRAME_SIZE];
static volatile uint8_t received_byte;
static volatile bool byte_ready;
static volatile bool frame_ready;
static volatile bool transmit_active;

static void clear_receive_fifo(void) {
    while (U2STAbits.URXDA != 0) {
        (void)U2RXREG;
    }
    U2STAbits.OERR = 0;
}

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

void platform_pedal_link_init(void) {
    byte_ready = false;
    frame_ready = false;
    transmit_active = false;
    configure_uart();
    configure_receive_dma();
    configure_transmit_dma();
    configure_interrupts();
    clear_receive_fifo();
}

void platform_pedal_link_begin_discovery(void) {
    IEC0bits.DMA1IE = 0;
    DMA1CONbits.CHEN = 0;
    ANSELBbits.ANSB13 = 0;
    ANSELBbits.ANSB14 = 0;
    U2MODEbits.UARTEN = 1;
    U2STAbits.UTXEN = 1;
    U2BRG = PEDAL_LEGACY_BAUD_PERIOD;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    IFS1bits.U2RXIF = 0;
    IEC1bits.U2RXIE = 1;
}

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
}

void platform_pedal_link_begin_framed_receive(void) {
    IEC1bits.U2RXIE = 0;
    DMA1CONbits.CHEN = 0;
    U2BRG = PEDAL_MODERN_BAUD_PERIOD;
    clear_receive_fifo();
    byte_ready = false;
    frame_ready = false;
    DMA1CNT = PEDAL_FRAME_SIZE - 1;
    IFS0bits.DMA1IF = 0;
    IEC0bits.DMA1IE = 1;
    DMA1CONbits.CHEN = 1;
}

static bool begin_send(uint8_t length) {
    IEC1bits.DMA2IE = 0;
    if (transmit_active) {
        IEC1bits.DMA2IE = 1;
        return false;
    }
    DMA2CONbits.CHEN = 0;
    DMA2CNT = length - 1;
    IFS1bits.DMA2IF = 0;
    transmit_active = true;
    DMA2CONbits.CHEN = 1;
    DMA2REQbits.FORCE = 1;
    IEC1bits.DMA2IE = 1;
    return true;
}

bool platform_pedal_link_send_byte(uint8_t value) {
    if (transmit_active) {
        return false;
    }
    transmitted_dma[0] = value;
    return begin_send(1);
}

bool platform_pedal_link_send_frame(const uint8_t frame[PEDAL_FRAME_SIZE]) {
    if (transmit_active) {
        return false;
    }
    for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
        transmitted_dma[index] = frame[index];
    }
    return begin_send(PEDAL_FRAME_SIZE);
}

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

void __attribute__((interrupt, no_auto_psv)) _U2RXInterrupt(void) {
    while (U2STAbits.URXDA != 0) {
        received_byte = (uint8_t)U2RXREG;
        byte_ready = true;
    }
    if (U2STAbits.OERR) {
        U2STAbits.OERR = 0;
    }
    IFS1bits.U2RXIF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _DMA1Interrupt(void) {
    for (uint8_t index = 0; index < PEDAL_FRAME_SIZE; index++) {
        received_frame[index] = received_dma[index];
    }
    frame_ready = true;
    DMA1CONbits.CHEN = 0;
    DMA1CONbits.CHEN = 1;
    IFS0bits.DMA1IF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _DMA2Interrupt(void) {
    transmit_active = false;
    IFS1bits.DMA2IF = 0;
}
