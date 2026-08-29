#ifndef OPENTEC_MOTOR_SPI_H
#define OPENTEC_MOTOR_SPI_H

#include <stdint.h>

#define MOTOR_SPI_TRANSFER_SIZE 13U

typedef struct {
    uint8_t transmit[MOTOR_SPI_TRANSFER_SIZE];
    uint8_t receive[MOTOR_SPI_TRANSFER_SIZE];
} MotorSpiTransferBuffers;

typedef void (*MotorSpiReceiveHandler)(const uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context);

void motor_spi_initialize(MotorSpiTransferBuffers *buffers, MotorSpiReceiveHandler receive_handler,
                          void *context);
void motor_spi_transfer_restart(void);

#endif
