#include "platform/spi.h"

#include <fsl_dmamux.h>
#include <fsl_dspi.h>
#include <fsl_edma.h>
#include <fsl_gpio.h>
#include <string.h>

/** @brief Persistent transmit and receive buffers used by both SPI DMA channels. */
static MotorSpiTransferBuffers *transfer_buffers;
/** @brief Callback that prepares a pending outgoing response frame. */
static MotorSpiPrepareHandler transfer_prepare_handler;
/** @brief Callback that processes each completed incoming frame. */
static MotorSpiReceiveHandler transfer_receive_handler;
/** @brief Context passed to the SPI transfer callbacks. */
static void *transfer_context;
/** @brief True when startup permits delayed SPI responses. */
static volatile bool transfer_active;
/** @brief True when the received frame needs a delayed response. */
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
 * @param[in] channel DMA and DMAMUX channel number.
 * @param[in] request Peripheral request source selected by the DMAMUX.
 * @param[in] transfer Prepared NXP SDK transfer descriptor.
 * @param[in] source_major_offset Source adjustment after each major loop.
 * @param[in] destination_major_offset Destination adjustment after each major loop.
 */
static void motor_spi_dma_channel_initialize(uint32_t channel, int32_t request,
                                             edma_transfer_config_t *transfer,
                                             int32_t source_major_offset,
                                             int32_t destination_major_offset) {
    DMAMUX_DisableChannel(DMAMUX, channel);
    EDMA_ResetChannel(DMA0, channel);
    EDMA_SetTransferConfig(DMA0, channel, transfer, NULL);
    EDMA_SetMajorOffsetConfig(DMA0, channel, source_major_offset, destination_major_offset);
    EDMA_EnableChannelInterrupts(DMA0, channel, kEDMA_MajorInterruptEnable);
    DMAMUX_SetSource(DMAMUX, channel, request);
    DMAMUX_EnableChannel(DMAMUX, channel);
}

/**
 * @brief Rebuilds both official thirteen-byte motor-link DMA descriptors.
 *
 * Startup installs the transmit descriptor before the receive descriptor. Recovery reverses this
 * order so the receive channel is ready before the next transmit begins.
 *
 * Channel zero transfers the response to SPI0 and channel one captures the simultaneous request.
 * Major-loop offsets return each descriptor to its persistent frame boundary.
 *
 * @param[in] receive_first True to install the receive descriptor before the transmit descriptor.
 */
static void motor_spi_dma_initialize(bool receive_first) {
    edma_transfer_config_t transfer;

    DMA0->CR = 0U;

    if (receive_first) {
        EDMA_PrepareTransfer(&transfer, (void *)&SPI0->POPR, 1U, transfer_buffers->receive, 1U, 1U,
                             MOTOR_SPI_TRANSFER_SIZE, kEDMA_PeripheralToMemory);
        motor_spi_dma_channel_initialize(1U, kDmaRequestMux0SPI0Rx, &transfer, 0,
                                         -((int32_t)MOTOR_SPI_TRANSFER_SIZE));

        EDMA_PrepareTransfer(&transfer, transfer_buffers->transmit, 1U, (void *)&SPI0->PUSHR, 1U,
                             1U, MOTOR_SPI_TRANSFER_SIZE, kEDMA_MemoryToPeripheral);
        motor_spi_dma_channel_initialize(0U, kDmaRequestMux0SPI0Tx, &transfer,
                                         -((int32_t)MOTOR_SPI_TRANSFER_SIZE), 0);
    } else {
        EDMA_PrepareTransfer(&transfer, transfer_buffers->transmit, 1U, (void *)&SPI0->PUSHR, 1U,
                             1U, MOTOR_SPI_TRANSFER_SIZE, kEDMA_MemoryToPeripheral);
        motor_spi_dma_channel_initialize(0U, kDmaRequestMux0SPI0Tx, &transfer,
                                         -((int32_t)MOTOR_SPI_TRANSFER_SIZE), 0);

        EDMA_PrepareTransfer(&transfer, (void *)&SPI0->POPR, 1U, transfer_buffers->receive, 1U, 1U,
                             MOTOR_SPI_TRANSFER_SIZE, kEDMA_PeripheralToMemory);
        motor_spi_dma_channel_initialize(1U, kDmaRequestMux0SPI0Rx, &transfer, 0,
                                         -((int32_t)MOTOR_SPI_TRANSFER_SIZE));
    }

    EDMA_EnableChannelRequest(DMA0, 1U);
    EDMA_EnableChannelRequest(DMA0, 0U);
}

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
    motor_spi_dma_initialize(false);

    EnableIRQ(DMA0_DMA4_IRQn);
    EnableIRQ(DMA1_DMA5_IRQn);
}

void motor_spi_link_active_set(bool active) { transfer_active = active; }

void motor_spi_timeout_service(void) {
    bool pending = response_pending;
    bool active = transfer_active;
    if (!pending || !active) {
        return;
    }

    response_pending = false;
    motor_spi_transfer_restart();
}

void motor_spi_transfer_restart(void) {
    if (!transfer_active) {
        return;
    }

    if (transfer_prepare_handler != NULL) {
        transfer_prepare_handler(transfer_buffers->transmit, transfer_context);
    }
    GPIOC->PDOR = ~(1UL << 0U);
    if ((SPI0->SR & SPI_SR_RXCTR_MASK) != 0U) {
        SPI0->MCR = SPI0->MCR & SPI_MCR_CLR_RXF_MASK;
        SPI0->POPR;
    }
    motor_spi_dma_initialize(true);
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
