#include "platform/motor_link.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

enum {
    MOTOR_LINK_DMA_REQUEST = 10,
    MOTOR_LINK_INTERRUPT_PRIORITY = 7,
};

static volatile uint8_t received_dma[PLATFORM_MOTOR_LINK_FRAME_SIZE];
static volatile uint8_t transmitted_dma[PLATFORM_MOTOR_LINK_FRAME_SIZE];
static volatile uint8_t received_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE];
static volatile uint8_t next_transmit[PLATFORM_MOTOR_LINK_FRAME_SIZE];
static volatile bool received_ready;

static void copy_to_volatile(volatile uint8_t *destination, const uint8_t *source) {
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        destination[index] = source[index];
    }
}

static void copy_from_volatile(uint8_t *destination, const volatile uint8_t *source) {
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        destination[index] = source[index];
    }
}

static void copy_between_volatile(volatile uint8_t *destination, const volatile uint8_t *source) {
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        destination[index] = source[index];
    }
}

static void configure_spi(void) {
    SPI1STATbits.SPIEN = 0;
    SPI1STATbits.SPIROV = 0;
    SPI1CON1 = 0;
    SPI1CON2 = 0;
    SPI1CON1bits.SSEN = 1;
    SPI1CON1bits.CKP = 0;
    SPI1CON1bits.CKE = 0;
    SPI1CON1bits.MSTEN = 0;
}

static void configure_dma(void) {
    DMA8CON = 0;
    DMA8CONbits.SIZE = 1;
    DMA8CONbits.DIR = 1;
    DMA8REQbits.IRQSEL = MOTOR_LINK_DMA_REQUEST;
    DMA8STAL = (uint16_t)transmitted_dma;
    DMA8STAH = 0;
    DMA8PAD = (uint16_t)&SPI1BUF;
    DMA8CNT = PLATFORM_MOTOR_LINK_FRAME_SIZE - 1;

    DMA9CON = 0;
    DMA9CONbits.SIZE = 1;
    DMA9CONbits.DIR = 0;
    DMA9REQbits.IRQSEL = MOTOR_LINK_DMA_REQUEST;
    DMA9STAL = (uint16_t)received_dma;
    DMA9STAH = 0;
    DMA9PAD = (uint16_t)&SPI1BUF;
    DMA9CNT = PLATFORM_MOTOR_LINK_FRAME_SIZE - 1;
}

static void configure_interrupts(void) {
    IPC2bits.SPI1IP = MOTOR_LINK_INTERRUPT_PRIORITY;
    IPC2bits.SPI1EIP = MOTOR_LINK_INTERRUPT_PRIORITY;
    IPC29bits.DMA8IP = MOTOR_LINK_INTERRUPT_PRIORITY;
    IPC29bits.DMA9IP = MOTOR_LINK_INTERRUPT_PRIORITY;
    IFS0bits.SPI1IF = 0;
    IFS0bits.SPI1EIF = 0;
    IFS7bits.DMA8IF = 0;
    IFS7bits.DMA9IF = 0;
    IEC0bits.SPI1IE = 1;
    IEC0bits.SPI1EIE = 1;
    IEC7bits.DMA8IE = 1;
    IEC7bits.DMA9IE = 1;
}

void platform_motor_link_init(const uint8_t initial_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]) {
    received_ready = false;
    copy_to_volatile(transmitted_dma, initial_frame);
    copy_to_volatile(next_transmit, initial_frame);
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        received_dma[index] = 0;
        received_frame[index] = 0;
    }

    TRISGbits.TRISG6 = 1;
    TRISGbits.TRISG7 = 1;
    TRISGbits.TRISG8 = 0;
    TRISGbits.TRISG9 = 1;

    configure_spi();
    configure_dma();
    configure_interrupts();
    SPI1STATbits.SPIEN = 1;
    DMA8CONbits.CHEN = 1;
    DMA9CONbits.CHEN = 1;
    DMA8REQbits.FORCE = 1;
}

void platform_motor_link_set_transmit(const uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]) {
    IEC7bits.DMA9IE = 0;
    copy_to_volatile(next_transmit, frame);
    IEC7bits.DMA9IE = 1;
}

bool platform_motor_link_take_received(uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]) {
    IEC7bits.DMA9IE = 0;
    bool ready = received_ready;
    if (ready) {
        copy_from_volatile(frame, received_frame);
        received_ready = false;
    }
    IEC7bits.DMA9IE = 1;
    return ready;
}

void __attribute__((interrupt, no_auto_psv)) _SPI1Interrupt(void) { IFS0bits.SPI1IF = 0; }

void __attribute__((interrupt, no_auto_psv)) _SPI1ErrInterrupt(void) {
    SPI1STATbits.SPIROV = 0;
    DMA9CONbits.CHEN = 0;
    DMA9CONbits.CHEN = 1;
    IFS0bits.SPI1EIF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _DMA8Interrupt(void) { IFS7bits.DMA8IF = 0; }

void __attribute__((interrupt, no_auto_psv)) _DMA9Interrupt(void) {
    copy_between_volatile(received_frame, received_dma);
    received_ready = true;

    DMA8CONbits.CHEN = 0;
    copy_between_volatile(transmitted_dma, next_transmit);
    DMA8CONbits.CHEN = 1;
    DMA8REQbits.FORCE = 1;
    IFS7bits.DMA9IF = 0;
}
