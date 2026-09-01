#ifndef OPENTEC_MOTOR_SPI_H
#define OPENTEC_MOTOR_SPI_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of bytes in one motor-link SPI transfer. */
#define MOTOR_SPI_TRANSFER_SIZE 13U

/**
 * @brief Holds the persistent transmit and receive buffers used by SPI DMA.
 */
typedef struct {
    uint8_t transmit[MOTOR_SPI_TRANSFER_SIZE]; /**< Frame transmitted to the motor controller. */
    uint8_t receive[MOTOR_SPI_TRANSFER_SIZE];  /**< Frame received from the motor controller. */
} MotorSpiTransferBuffers;

/**
 * @brief Prepares one outgoing motor-link frame.
 *
 * The callback fills the persistent transmit buffer immediately before a DMA transfer restarts.
 *
 * @param[out] frame Persistent transmit frame buffer to fill.
 * @param[in] context Caller context supplied during SPI initialization.
 */
typedef void (*MotorSpiPrepareHandler)(uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context);

/**
 * @brief Processes one received motor-link frame.
 *
 * The callback validates and applies the frame, then reports whether the response scheduler should
 * start another transfer.
 *
 * @param[in] frame Persistent receive frame buffer.
 * @param[in] context Caller context supplied during SPI initialization.
 * @return True when a delayed response should be scheduled.
 */
typedef bool (*MotorSpiReceiveHandler)(const uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context);

/**
 * @brief Initializes the SPI controller and its two DMA channels.
 *
 * The supplied persistent buffers and callbacks are installed before SPI transfer interrupts are
 * enabled.
 *
 * @param[out] buffers Persistent transmit and receive buffers used directly by DMA.
 * @param[in] prepare_handler Function that prepares the next response frame.
 * @param[in] receive_handler Function that processes a completed receive frame.
 * @param[in] context Caller context passed to both handlers.
 */
void motor_spi_initialize(MotorSpiTransferBuffers *buffers, MotorSpiPrepareHandler prepare_handler,
                          MotorSpiReceiveHandler receive_handler, void *context);

/**
 * @brief Enables or disables delayed motor-link response scheduling.
 *
 * Responses remain blocked until startup alignment and encoder setup are complete.
 *
 * @param[in] active True to allow delayed link responses; false to block them.
 */
void motor_spi_link_active_set(bool active);

/**
 * @brief Services one delayed motor-link response timer event.
 *
 * An active pending response is prepared and both DMA channels are restarted.
 *
 * @param[in] context Timer callback context supplied by the communication timer.
 */
void motor_spi_timeout_service(void *context);

/**
 * @brief Restarts both motor-link SPI DMA transfers.
 *
 * Chip select is asserted, stale receive data is flushed, and fresh transfer descriptors are
 * installed.
 */
void motor_spi_transfer_restart(void);

#endif
