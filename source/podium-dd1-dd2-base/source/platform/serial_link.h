#ifndef OPENTEC_BASE_PLATFORM_SERIAL_LINK_H
#define OPENTEC_BASE_PLATFORM_SERIAL_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/packet.h"

/** @brief Framed receive-DMA storage size including the four-byte alignment window. */
enum { PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE = SERIAL_PACKET_SIZE + 4 };

/**
 * @brief Initializes the serial device link.
 *
 * Configures UART3, its DMA channels, and the timing interrupt used by framed exchanges. UART3 is
 * enabled only after the DMA descriptors and interrupt gates are configured; receive DMA remains
 * disarmed until a framed exchange starts.
 */
void platform_serial_link_init(void);

/**
 * @brief Resets the framed serial device link.
 *
 * Stops active framed transfers, clears receive state, and leaves direct mode unchanged.
 */
void platform_serial_link_reset(void);

/**
 * @brief Starts one framed serial request and response exchange.
 *
 * Queues the supplied transport packet when direct mode is inactive and no other framed exchange is
 * active.
 *
 * @param[in] packet Transport packet to transmit.
 * @return True when the exchange was started; otherwise false.
 */
bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]);

/**
 * @brief Takes one framed serial receive result.
 *
 * Copies and consumes the newest response packet or invalid timeout result when framed mode has a
 * result ready. Direct mode never provides a framed result.
 *
 * @param[out] packet Destination for the received transport packet.
 * @return True when a response was copied; otherwise false.
 */
bool platform_serial_link_take_received(uint8_t packet[SERIAL_PACKET_SIZE]);

/**
 * @brief Starts the periodic framed-receive recovery interval.
 *
 * Holds new framed exchanges until the official 0x27d8-cycle Timer 6 recovery interval expires.
 * Direct mode and active or already-pending framed exchanges reject the request.
 *
 * @return True when recovery was scheduled; otherwise false while the link is busy or in direct
 * mode.
 */
bool platform_serial_link_start_periodic_recovery(void);

/**
 * @brief Enters raw direct serial mode.
 *
 * Stops framed DMA exchanges and enables the interrupt-driven updater byte stream.
 */
void platform_serial_link_enter_direct_mode(void);

/**
 * @brief Queues raw direct-mode serial bytes.
 *
 * Retains one bounded, nonempty request for transmission by the direct-mode service when direct
 * mode is active and no request is already pending.
 *
 * @param[in] data Request bytes to transmit.
 * @param[in] length Number of request bytes.
 * @return True when the request was queued; otherwise false.
 */
bool platform_serial_link_direct_write(const uint8_t *data, uint8_t length);

/**
 * @brief Reads raw direct-mode serial bytes.
 *
 * Copies and consumes a complete requested fragment in direct mode when enough response bytes are
 * buffered. A zero-length fragment completes immediately.
 *
 * @param[out] data Destination for the response fragment.
 * @param[in] length Number of response bytes to read.
 * @return True when the fragment was copied; otherwise false.
 */
bool platform_serial_link_direct_read(uint8_t *data, uint8_t length);

/**
 * @brief Clears retained direct-mode serial response bytes.
 *
 * Leaves direct mode active while discarding the current response assembly.
 */
void platform_serial_link_direct_clear(void);

#ifdef OPENTEC_SIMULATOR_TEST
/**
 * @brief Loads a simulated framed receive-DMA buffer.
 *
 * @param[in] frame 68 bytes to expose through the receive-DMA completion interrupt.
 */
void platform_serial_link_test_set_receive_dma(
    const uint8_t frame[PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE]);
#endif

#endif
