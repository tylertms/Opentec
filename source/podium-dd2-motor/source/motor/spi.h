#ifndef OPENTEC_MOTOR_SPI_H
#define OPENTEC_MOTOR_SPI_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_SPI_TRANSFER_SIZE 13U

typedef struct {
    uint8_t transmit[MOTOR_SPI_TRANSFER_SIZE];
    uint8_t receive[MOTOR_SPI_TRANSFER_SIZE];
} MotorSpiTransferBuffers;

typedef void (*MotorSpiPrepareHandler)(uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context);
typedef bool (*MotorSpiReceiveHandler)(const uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context);

void motor_spi_initialize(MotorSpiTransferBuffers *buffers, MotorSpiPrepareHandler prepare_handler,
                          MotorSpiReceiveHandler receive_handler, void *context);
void motor_spi_link_active_set(bool active);
void motor_spi_timeout_service(void *context);
void motor_spi_transfer_restart(void);

#endif
