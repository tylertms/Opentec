#include "common/motor/spi.h"

#include <fsl_dspi.h>

/**
 * @brief Configures SPI0 as an eight-bit DMA-driven motor peripheral controller.
 */
void motor_spi_controller_initialize(void) {
    DisableIRQ(DMA0_DMA4_IRQn);
    DisableIRQ(DMA1_DMA5_IRQn);
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

    EnableIRQ(DMA0_DMA4_IRQn);
    EnableIRQ(DMA1_DMA5_IRQn);
}
