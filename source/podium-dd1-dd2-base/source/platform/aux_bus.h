#ifndef OPENTEC_BASE_PLATFORM_AUX_BUS_H
#define OPENTEC_BASE_PLATFORM_AUX_BUS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State of an auxiliary-bus transaction.
 */
typedef enum {
    PLATFORM_AUX_BUS_IDLE,      /**< No transaction is active. */
    PLATFORM_AUX_BUS_BUSY,      /**< A transaction is in progress. */
    PLATFORM_AUX_BUS_SUCCEEDED, /**< The most recent transaction succeeded. */
    PLATFORM_AUX_BUS_FAILED,    /**< The most recent transaction failed. */
} PlatformAuxBusStatus;

/**
 * @brief Initializes the auxiliary bus controller.
 *
 * Releases a stalled bus when needed, enables the I2C controller and interrupt service, and
 * consumes the controller-reset stop without starting a transaction.
 */
void platform_aux_bus_init(void);

/**
 * @brief Services the foreground auxiliary-bus polling hook.
 *
 * Timer 1 owns the active transfer timeout. This hook remains safe to call from polling loops and
 * does not consume timeout ticks.
 */
void platform_aux_bus_service(void);

/**
 * @brief Advances the auxiliary-bus timeout from a one-millisecond timer interrupt.
 *
 * Resets the controller and publishes failure when an active transfer misses two consecutive
 * Timer 1 ticks without an I2C event.
 */
void platform_aux_bus_timer_tick(void);

/**
 * @brief Starts a register-addressed auxiliary-bus write.
 *
 * Queues a payload write, or a register-only write when data is null and length is zero.
 *
 * @param[in] address Seven-bit device address.
 * @param[in] register_address Eight- or sixteen-bit register address.
 * @param[in] data Payload source, or null for a register-only write.
 * @param[in] length Number of payload bytes.
 * @return True when the transaction was started; otherwise false.
 */
bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length);

/**
 * @brief Starts a register-addressed auxiliary-bus read.
 *
 * Queues a read into the supplied destination buffer. A zero-length read still performs the
 * register-addressed remote transaction.
 *
 * @param[in] address Seven-bit device address.
 * @param[in] register_address Eight- or sixteen-bit register address.
 * @param[out] data Destination for received bytes.
 * @param[in] length Number of bytes to receive.
 * @return True when the transaction was started; otherwise false.
 */
bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length);

/**
 * @brief Reads the auxiliary-bus transaction status.
 *
 * Reports whether the controller is idle, busy, succeeded, or failed.
 *
 * @return Current auxiliary-bus status.
 */
PlatformAuxBusStatus platform_aux_bus_status(void);

/**
 * @brief Clears a completed auxiliary-bus status.
 *
 * Returns a completed status to idle without changing an active transaction.
 */
void platform_aux_bus_clear(void);

/**
 * @brief Aborts an active auxiliary-bus transaction.
 *
 * Resets the I2C controller, releases the bus, and returns the shared transaction status to idle.
 * Completed statuses remain available to their owner until platform_aux_bus_clear() is called.
 */
void platform_aux_bus_cancel(void);

#ifdef OPENTEC_SIMULATOR_TEST
/**
 * @brief Reads the retry count after a simulated auxiliary-bus transaction.
 *
 * Exposes the bounded-retry state to simulator regression tests without adding a production API.
 *
 * @return Number of NACK retries used by the active or most recent transaction.
 */
uint8_t platform_aux_bus_retry_count(void);
#endif

#endif
