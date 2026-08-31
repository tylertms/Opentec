#include "platform/spi.h"

#include <fsl_dmamux.h>
#include <fsl_dspi.h>
#include <fsl_edma.h>
#include <fsl_gpio.h>
#include <string.h>

static MotorSpiTransferBuffers *transfer_buffers;
static MotorSpiPrepareHandler transfer_prepare_handler;
static MotorSpiReceiveHandler transfer_receive_handler;
static void *transfer_context;
static volatile bool transfer_active;
static volatile bool response_pending;

/**
 * @brief Configures the official SPI0 motor-link controller.
 *
 * SPI0 operates as an eight-bit full-duplex controller with disabled FIFOs and DMA requests for
 * both transmit and receive data.
 */
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

/**
 * @brief Configures one official motor-link DMA channel.
 *
 * The channel is reset, loaded with its transfer descriptor, connected to its DMAMUX request, and
 * enabled with a major-loop interrupt.
 *
 * @param channel DMA and DMAMUX channel number.
 * @param request Peripheral request source selected by the DMAMUX.
 * @param transfer Prepared NXP SDK transfer descriptor.
 */
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
 * @brief Rebuilds both official thirteen-byte motor-link DMA descriptors.
 *
 * Channel zero transfers the response to SPI0 and channel one captures the simultaneous request.
 */
static void motor_spi_dma_initialize(void) {
    edma_transfer_config_t transfer;

    DMA0->CR = 0U;

    EDMA_PrepareTransfer(&transfer, transfer_buffers->transmit, 1U, (void *)&SPI0->PUSHR, 1U, 1U,
                         MOTOR_SPI_TRANSFER_SIZE, kEDMA_MemoryToPeripheral);
    motor_spi_dma_channel_initialize(0U, kDmaRequestMux0SPI0Tx, &transfer);

    EDMA_PrepareTransfer(&transfer, (void *)&SPI0->POPR, 1U, transfer_buffers->receive, 1U, 1U,
                         MOTOR_SPI_TRANSFER_SIZE, kEDMA_PeripheralToMemory);
    motor_spi_dma_channel_initialize(1U, kDmaRequestMux0SPI0Rx, &transfer);

    EDMA_EnableChannelRequest(DMA0, 1U);
    EDMA_EnableChannelRequest(DMA0, 0U);
}

/**
 * @brief Configures SPI0 and two 13-byte DMA channels for full-duplex motor transfers.
 *
 * Persistent buffers and callbacks are installed before the controller, DMAMUX, DMA engine, and
 * channel interrupts are enabled.
 *
 * @param buffers Persistent transmit and receive buffers used directly by DMA.
 * @param prepare_handler Function that prepares the next response before a scheduled transfer.
 * @param receive_handler Function that processes a completed receive and approves its response.
 * @param context Caller context passed to the receive handler.
 */
void motor_spi_initialize(MotorSpiTransferBuffers *buffers, MotorSpiPrepareHandler prepare_handler,
                          MotorSpiReceiveHandler receive_handler, void *context) {
    memset(buffers, 0, sizeof(*buffers));
    transfer_buffers = buffers;
    transfer_prepare_handler = prepare_handler;
    transfer_receive_handler = receive_handler;
    transfer_context = context;
    transfer_active = false;
    response_pending = false;
    DisableIRQ(DMA0_DMA4_IRQn);
    DisableIRQ(DMA1_DMA5_IRQn);

    motor_spi_controller_initialize();
    CLOCK_EnableClock(kCLOCK_Dmamux0);
    CLOCK_EnableClock(kCLOCK_Dma0);
    DMA0->CR = 0U;

    motor_spi_dma_initialize();

    EnableIRQ(DMA0_DMA4_IRQn);
    EnableIRQ(DMA1_DMA5_IRQn);
}

/**
 * @brief Enables or disables the official delayed motor-link response scheduler.
 *
 * Link responses remain blocked until startup alignment and encoder setup complete.
 *
 * @param active True after motor startup permits link responses.
 */
void motor_spi_link_active_set(bool active) { transfer_active = active; }

/**
 * @brief Starts one pending motor-link response on the official FTM4 cadence.
 *
 * An active pending response is rebuilt in the transmit buffer before both DMA channels restart.
 *
 * @param context Unused timer callback context.
 */
void motor_spi_timeout_service(void *context) {
    (void)context;
    bool pending = response_pending;
    bool active = transfer_active;
    if (!pending || !active) {
        return;
    }

    response_pending = false;
    if (transfer_prepare_handler != NULL) {
        transfer_prepare_handler(transfer_buffers->transmit, transfer_context);
    }
    motor_spi_transfer_restart();
}

/**
 * @brief Rebuilds and enables both official thirteen-byte SPI DMA transfers.
 *
 * Chip select is asserted, stale receive data is flushed, and fresh transmit and receive transfer
 * descriptors are installed.
 */
void motor_spi_transfer_restart(void) {
    GPIO_PortClear(GPIOC, 1UL << 0U);
    if ((SPI0->SR & SPI_SR_RXCTR_MASK) != 0U) {
        DSPI_FlushFifo(SPI0, false, true);
        (void)SPI0->POPR;
    }
    motor_spi_dma_initialize();
}

/**
 * @brief Completes the official SPI transmit DMA channel interrupt.
 *
 * Channel-zero interrupt and completion flags are acknowledged together.
 */
void DMA0_DMA4_IRQHandler(void) {
    EDMA_ClearChannelStatusFlags(DMA0, 0U, kEDMA_InterruptFlag | kEDMA_DoneFlag);
}

/**
 * @brief Completes the official SPI receive DMA channel and processes its frame.
 *
 * Channel-one completion releases chip select and records whether the decoded request schedules a
 * delayed response.
 */
void DMA1_DMA5_IRQHandler(void) {
    EDMA_ClearChannelStatusFlags(DMA0, 1U, kEDMA_InterruptFlag | kEDMA_DoneFlag);
    GPIO_PortSet(GPIOC, 1UL << 0U);
    response_pending = transfer_receive_handler == NULL ||
                       transfer_receive_handler(transfer_buffers->receive, transfer_context);
}
