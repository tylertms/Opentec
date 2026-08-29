#ifndef OPENTEC_MOTOR_SPI_H
#define OPENTEC_MOTOR_SPI_H

#include <stdint.h>

#define MOTOR_SPI_TRANSFER_SIZE 13U

typedef struct {
    uint8_t transmit[MOTOR_SPI_TRANSFER_SIZE];
    uint8_t receive[MOTOR_SPI_TRANSFER_SIZE];
} MotorSpiTransferBuffers;

void motor_spi_initialize(MotorSpiTransferBuffers *buffers);

#endif
