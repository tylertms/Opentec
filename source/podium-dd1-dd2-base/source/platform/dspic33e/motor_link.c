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

/**
 * @brief Copies one motor-link frame into volatile storage.
 *
 * Copies all thirteen frame bytes from foreground storage to an interrupt-shared destination.
 *
 * @param[out] destination Interrupt-shared frame destination.
 * @param[in] source Foreground frame source.
 */
static void copy_to_volatile(volatile uint8_t *destination, const uint8_t *source) {
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        destination[index] = source[index];
    }
}

/**
 * @brief Copies one motor-link frame out of volatile storage.
 *
 * Copies all thirteen frame bytes from an interrupt-shared source to a foreground destination.
 *
 * @param[out] destination Foreground frame destination.
 * @param[in] source Interrupt-shared frame source.
 */
static void copy_from_volatile(uint8_t *destination, const volatile uint8_t *source) {
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        destination[index] = source[index];
    }
}

/**
 * @brief Copies one motor-link frame between volatile buffers.
 *
 * Copies all thirteen frame bytes between DMA and interrupt-owned storage.
 *
 * @param[out] destination Interrupt-shared frame destination.
 * @param[in] source Interrupt-shared frame source.
 */
static void copy_between_volatile(volatile uint8_t *destination, const volatile uint8_t *source) {
    for (uint8_t index = 0; index < PLATFORM_MOTOR_LINK_FRAME_SIZE; index++) {
        destination[index] = source[index];
    }
}

/**
 * @brief Configures SPI1 as the motor controller's slave transport.
 *
 * Disables the controller, clears receive overflow, and selects slave operation with active-low
 * chip select, idle-low clock, and input sampling on the falling edge.
 */
static void configure_spi(void) {
    SPI1STATbits.SPIEN = 0;
    SPI1STATbits.SPISIDL = 0;
    SPI1STATbits.SPIBEC = 0;
    SPI1STATbits.SRMPT = 0;
    SPI1STATbits.SRXMPT = 0;
    SPI1STATbits.SISEL = 0;
    SPI1STATbits.SPIROV = 0;
    SPI1CON1 = 0;
    SPI1CON2 = 0;
    SPI1CON1bits.SSEN = 1;
    SPI1CON1bits.CKP = 0;
    SPI1CON1bits.CKE = 0;
    SPI1CON1bits.MSTEN = 0;
}

/**
 * @brief Configures byte-wide DMA for motor-link transmission and reception.
 *
 * Assigns DMA8 to transmit and DMA9 to receive thirteen-byte SPI1 frames using peripheral request
 * number ten.
 */
static void configure_dma(void) {
    DMA8CON = 0;
    DMA8CONbits.SIZE = 1;
    DMA8CONbits.DIR = 1;
    DMA8REQbits.FORCE = 0;
    DMA8REQbits.IRQSEL = MOTOR_LINK_DMA_REQUEST;
    DMA8STAL = (uint16_t)transmitted_dma;
    DMA8STAH = 0;
    DMA8PAD = (uint16_t)&SPI1BUF;
    DMA8CNT = PLATFORM_MOTOR_LINK_FRAME_SIZE - 1;

    DMA9CON = 0;
    DMA9CONbits.SIZE = 1;
    DMA9CONbits.DIR = 0;
    DMA9REQbits.FORCE = 0;
    DMA9REQbits.IRQSEL = MOTOR_LINK_DMA_REQUEST;
    DMA9STAL = (uint16_t)received_dma;
    DMA9STAH = 0;
    DMA9PAD = (uint16_t)&SPI1BUF;
    DMA9CNT = PLATFORM_MOTOR_LINK_FRAME_SIZE - 1;
}

/**
 * @brief Configures motor-link SPI and DMA interrupts.
 *
 * Clears pending requests, keeps overflow recovery disabled until frame synchronization, and
 * enables the SPI1, DMA8, and DMA9 interrupts at priority seven.
 */
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
    IEC0bits.SPI1EIE = 0;
    IEC7bits.DMA8IE = 1;
    IEC7bits.DMA9IE = 1;
}

/**
 * @brief Initializes the interrupt-driven motor SPI transport.
 *
 * Initializes both transmit buffers from the supplied frame, clears receive state, configures the
 * four SPI pins, controller, DMA channels, and interrupts, then primes the first transmission.
 *
 * @param[in] initial_frame First thirteen-byte frame presented to the motor controller.
 */
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

/**
 * @brief Enables motor-link overflow recovery after frame synchronization.
 *
 * Clears any stale SPI1 error request and permits the error handler only after the protocol layer
 * accepts a complete frame.
 *
 */
void platform_motor_link_confirm_synchronized(void) {
    if (IEC0bits.SPI1EIE == 0) {
        IFS0bits.SPI1EIF = 0;
        IEC0bits.SPI1EIE = 1;
    }
}

/**
 * @brief Queues the next motor-link transmit frame.
 *
 * Replaces both pending and active thirteen-byte frame storage, then restarts DMA8 immediately
 * while excluding the DMA9 completion handler.
 *
 * @param[in] frame Frame to transmit after the current exchange.
 */
void platform_motor_link_set_transmit(const uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]) {
    IEC7bits.DMA9IE = 0;
    copy_to_volatile(next_transmit, frame);
    DMA8CONbits.CHEN = 0;
    copy_to_volatile(transmitted_dma, frame);
    DMA8CONbits.CHEN = 1;
    DMA8REQbits.FORCE = 1;
    IEC7bits.DMA9IE = 1;
}

/**
 * @brief Retrieves the most recently completed motor-link receive frame.
 *
 * Copies and consumes the completed thirteen-byte frame while excluding the DMA9 completion
 * handler. Returns immediately when no frame is pending.
 *
 * @param[out] frame Destination for the received frame.
 * @return True when a completed frame was copied; otherwise false.
 */
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

/**
 * @brief Clears a normal SPI1 interrupt request.
 *
 * Acknowledges the interrupt because motor-link traffic is transferred exclusively by DMA.
 */
void __attribute__((interrupt, no_auto_psv)) _SPI1Interrupt(void) { IFS0bits.SPI1IF = 0; }

/**
 * @brief Recovers the motor-link receiver from a SPI1 overflow.
 *
 * Clears receive-full and overflow state, restarts DMA9, and acknowledges the SPI1 error
 * interrupt.
 */
void __attribute__((interrupt, no_auto_psv)) _SPI1ErrInterrupt(void) {
    SPI1STATbits.SPIROV = 0;
    SPI1STATbits.SPIRBF = 0;
    DMA9CONbits.CHEN = 0;
    DMA9CONbits.CHEN = 1;
    IFS0bits.SPI1EIF = 0;
}

/**
 * @brief Clears a completed motor-link transmit request.
 *
 * Acknowledges DMA8 completion after the queued frame has left the transmit buffer.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA8Interrupt(void) { IFS7bits.DMA8IF = 0; }

/**
 * @brief Publishes a received motor frame and stops the completed exchange.
 *
 * Snapshots the completed receive frame, marks it available to the main loop, disables DMA8, and
 * acknowledges the DMA9 completion request. The next transmit setter restarts the exchange.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA9Interrupt(void) {
    copy_between_volatile(received_frame, received_dma);
    received_ready = true;
    DMA8CONbits.CHEN = 0;
    IFS7bits.DMA9IF = 0;
}
