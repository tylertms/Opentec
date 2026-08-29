#include "common/motor/spi.h"

#include <fsl_dmamux.h>
#include <fsl_dspi.h>
#include <fsl_edma.h>
#include <string.h>

static void motor_spi_controller_initialize(void) {
    CLOCK_EnableClock(kCLOCK_Spi0);

    SPI0->MCR = SPI_MCR_MSTR_MASK | SPI_MCR_PCSIS(1U) | SPI_MCR_DIS_TXF_MASK |
                SPI_MCR_DIS_RXF_MASK | SPI_MCR_HALT_MASK;
    SPI0->TCR = 0U;
    SPI0->CTAR[0] = SPI_CTAR_FMSZ(7U) | SPI_CTAR_CPHA_MASK | SPI_CTAR_PCSSCK(1U) |
                    SPI_CTAR_PASC(1U) | SPI_CTAR_PBR(1U) | SPI_CTAR_CSSCK(4U) | SPI_CTAR_ASC(2U) |
                    SPI_CTAR_BR(4U);
    SPI0->SR = SPI_SR_TCF_MASK | SPI_SR_TXRXS_MASK | SPI_SR_EOQF_MASK | SPI_SR_TFUF_MASK |
               SPI_SR_TFFF_MASK | SPI_SR_RFOF_MASK | SPI_SR_RFDF_MASK;
    SPI0->RSER = SPI_RSER_TFFF_RE_MASK | SPI_RSER_TFFF_DIRS_MASK | SPI_RSER_RFDF_RE_MASK |
                 SPI_RSER_RFDF_DIRS_MASK;
    SPI0->MCR &= ~SPI_MCR_HALT_MASK;
}

static void motor_spi_dma_channel_initialize(uint32_t channel, int32_t request,
                                             edma_transfer_config_t *transfer) {
    DMAMUX_DisableChannel(DMAMUX, channel);
    EDMA_ResetChannel(DMA0, channel);
    EDMA_SetTransferConfig(DMA0, channel, transfer, NULL);
    EDMA_EnableChannelInterrupts(DMA0, channel, kEDMA_MajorInterruptEnable);
    DMAMUX_SetSource(DMAMUX, channel, request);
    DMAMUX_EnableChannel(DMAMUX, channel);
}

/**
 * @brief Configures SPI0 and two 13-byte DMA channels for full-duplex motor transfers.
 * @param buffers Persistent transmit and receive buffers used directly by DMA.
 */
void motor_spi_initialize(MotorSpiTransferBuffers *buffers) {
    edma_transfer_config_t transfer;

    memset(buffers, 0, sizeof(*buffers));
    DisableIRQ(DMA0_DMA4_IRQn);
    DisableIRQ(DMA1_DMA5_IRQn);

    motor_spi_controller_initialize();
    CLOCK_EnableClock(kCLOCK_Dmamux0);
    CLOCK_EnableClock(kCLOCK_Dma0);
    DMA0->CR = 0U;

    EDMA_PrepareTransfer(&transfer, buffers->transmit, 1U, (void *)&SPI0->PUSHR, 1U, 1U,
                         MOTOR_SPI_TRANSFER_SIZE, kEDMA_MemoryToPeripheral);
    motor_spi_dma_channel_initialize(0U, kDmaRequestMux0SPI0Tx, &transfer);

    EDMA_PrepareTransfer(&transfer, (void *)&SPI0->POPR, 1U, buffers->receive, 1U, 1U,
                         MOTOR_SPI_TRANSFER_SIZE, kEDMA_PeripheralToMemory);
    motor_spi_dma_channel_initialize(1U, kDmaRequestMux0SPI0Rx, &transfer);

    EnableIRQ(DMA0_DMA4_IRQn);
    EnableIRQ(DMA1_DMA5_IRQn);
}
