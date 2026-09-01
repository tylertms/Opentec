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
 * Releases a stalled bus when needed and enables the I2C controller and interrupt service.
 */
void platform_aux_bus_init(void);

/**
 * @brief Services auxiliary-bus transaction timeouts.
 *
 * Resets the controller and publishes failure when an active transaction exceeds its deadline.
 */
void platform_aux_bus_service(void);

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
 * Queues a nonempty read into the supplied destination buffer.
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

#endif
