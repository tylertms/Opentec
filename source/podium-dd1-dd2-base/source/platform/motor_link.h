#ifndef OPENTEC_BASE_PLATFORM_MOTOR_LINK_H
#define OPENTEC_BASE_PLATFORM_MOTOR_LINK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Number of bytes in one motor-link SPI frame.
 */
enum {
    PLATFORM_MOTOR_LINK_FRAME_SIZE = 13 /**< Number of bytes in one motor-link SPI frame. */
};

/**
 * @brief Initializes the motor-link SPI transport.
 *
 * Configures SPI1 and DMA and primes transmission with the supplied initial frame.
 *
 * @param[in] initial_frame First frame to transmit.
 */
void platform_motor_link_init(const uint8_t initial_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);

/**
 * @brief Enables motor-link overflow recovery.
 *
 * Marks the transport synchronized so SPI error interrupts may recover receive overflow.
 */
void platform_motor_link_confirm_synchronized(void);

/**
 * @brief Queues the next motor-link transmit frame.
 *
 * Replaces the pending frame loaded by the next DMA9 completion interrupt.
 *
 * @param[in] frame Frame to transmit.
 */
void platform_motor_link_set_transmit(const uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);

/**
 * @brief Takes one completed motor-link receive frame.
 *
 * Copies and consumes the oldest completed frame. Up to two completed frames can be retained while
 * the foreground is busy.
 *
 * @param[out] frame Destination for the received frame.
 * @return True when a frame was available; otherwise false.
 */
bool platform_motor_link_take_received(uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);

#ifdef OPENTEC_SIMULATOR_TEST
/**
 * @brief Loads a simulated motor-link receive-DMA frame.
 *
 * @param[in] frame Thirteen bytes to expose through the DMA9 completion interrupt.
 */
void platform_motor_link_test_set_receive_dma(const uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE]);
#endif

#endif
