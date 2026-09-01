#ifndef OPENTEC_BASE_PLATFORM_PEDAL_LINK_H
#define OPENTEC_BASE_PLATFORM_PEDAL_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"
#include "transfer/frame.h"

/**
 * @brief Initializes the pedal communication link.
 *
 * Configures UART2, its receive and transmit DMA channels, and the interrupt state used by pedal
 * discovery and data exchanges.
 */
void platform_pedal_link_init(void);

/**
 * @brief Selects analog pedal input mode.
 *
 * Stops serial reception and configures the pedal signal pins as analog inputs.
 */
void platform_pedal_link_begin_analog(void);

/**
 * @brief Selects byte-oriented pedal discovery mode.
 *
 * Configures the legacy serial rate and enables byte reception for discovery responses.
 */
void platform_pedal_link_begin_discovery(void);

/**
 * @brief Selects fixed-size framed pedal reception.
 *
 * Configures the modern serial rate and arms DMA for one PEDAL_FRAME_SIZE-byte frame.
 */
void platform_pedal_link_begin_framed_receive(void);

/**
 * @brief Selects variable-length pedal transfer reception.
 *
 * Configures byte-oriented collection of escaped transfer frames.
 */
void platform_pedal_link_begin_transfer_receive(void);

/**
 * @brief Sends one pedal command byte.
 *
 * Queues a one-byte UART transmission when the pedal transmitter is idle.
 *
 * @param[in] value Command byte to transmit.
 * @return True when transmission started; otherwise false.
 */
bool platform_pedal_link_send_byte(uint8_t value);

/**
 * @brief Sends one fixed-size pedal frame.
 *
 * Copies and transmits one PEDAL_FRAME_SIZE-byte encoded frame when the pedal transmitter is idle.
 *
 * @param[in] frame Encoded pedal frame to transmit.
 * @return True when transmission started; otherwise false.
 */
bool platform_pedal_link_send_frame(const uint8_t frame[PEDAL_FRAME_SIZE]);

/**
 * @brief Sends one variable-length pedal transfer frame.
 *
 * Copies and transmits one encoded transfer frame that fits the platform buffer.
 *
 * @param[in] data Encoded transfer frame to transmit.
 * @param[in] length Number of encoded bytes.
 * @return True when transmission started; otherwise false.
 */
bool platform_pedal_link_send_transfer(const uint8_t *data, uint16_t length);

/**
 * @brief Reports whether pedal transmission is active.
 *
 * Reads the state cleared by transmit-DMA completion.
 *
 * @return True while transmission is active; otherwise false.
 */
bool platform_pedal_link_transmit_busy(void);

/**
 * @brief Takes the newest byte-oriented pedal response.
 *
 * Copies and consumes the pending discovery or legacy response byte.
 *
 * @param[out] value Destination for the received byte.
 * @return True when a byte was consumed; otherwise false.
 */
bool platform_pedal_link_take_byte(uint8_t *value);

/**
 * @brief Takes one completed fixed-size pedal frame.
 *
 * Copies and consumes the newest boundary-aligned PEDAL_FRAME_SIZE-byte frame.
 *
 * @param[out] frame Destination for the received frame.
 * @return True when a frame was consumed; otherwise false.
 */
bool platform_pedal_link_take_frame(uint8_t frame[PEDAL_FRAME_SIZE]);

/**
 * @brief Takes one complete variable-length pedal transfer.
 *
 * Copies and consumes the oldest queued transfer when it fits the destination capacity.
 *
 * @param[out] data Destination for the encoded transfer frame.
 * @param[in] capacity Number of bytes available in data.
 * @return Encoded frame length, or zero when no complete frame fits.
 */
uint16_t platform_pedal_link_take_transfer(uint8_t *data, uint16_t capacity);

#endif
