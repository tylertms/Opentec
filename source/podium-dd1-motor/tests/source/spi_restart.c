#include <assert.h>
#include <stdint.h>

#include <fsl_dmamux.h>
#include <fsl_dspi.h>
#include <fsl_edma.h>

#include "platform/spi.h"

static uint32_t prepare_calls;
static uint32_t prepare_chip_select;

typedef struct {
    uint8_t seed;
    uint32_t receive_calls;
} TransferContext;

void DMA1_DMA5_IRQHandler(void);

static void prepare_frame(uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context) {
    const TransferContext *transfer = context;
    const uint8_t seed = transfer->seed;
    ++prepare_calls;
    prepare_chip_select = GPIOC->PDOR;
    for (uint32_t index = 0U; index < MOTOR_SPI_TRANSFER_SIZE; ++index) {
        frame[index] = (uint8_t)(seed + index);
    }
}

static bool receive_frame(const uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context) {
    TransferContext *transfer = context;
    ++transfer->receive_calls;
    return frame[0] != 0U;
}

static void assert_dma_layout(const MotorSpiTransferBuffers *buffers) {
    const uint32_t transfer_size = MOTOR_SPI_TRANSFER_SIZE;
    const uint32_t transmit_address = (uint32_t)(uintptr_t)buffers->transmit;
    const uint32_t receive_address = (uint32_t)(uintptr_t)buffers->receive;
    const uint32_t push_address = (uint32_t)(uintptr_t)&SPI0->PUSHR;
    const uint32_t pop_address = (uint32_t)(uintptr_t)&SPI0->POPR;
    const uint16_t completion = DMA_CSR_INTMAJOR_MASK | DMA_CSR_DREQ_MASK;

    assert(DMA0->TCD[0].SADDR == transmit_address);
    assert(DMA0->TCD[0].SOFF == 1U);
    assert(DMA0->TCD[0].ATTR == 0U);
    assert(DMA0->TCD[0].NBYTES_MLNO == 1U);
    assert(DMA0->TCD[0].SLAST == -((int32_t)transfer_size));
    assert(DMA0->TCD[0].DADDR == push_address);
    assert(DMA0->TCD[0].DOFF == 0U);
    assert(DMA0->TCD[0].CITER_ELINKNO == transfer_size);
    assert(DMA0->TCD[0].DLAST_SGA == 0);
    assert(DMA0->TCD[0].CSR == completion);
    assert(DMA0->TCD[0].BITER_ELINKNO == transfer_size);

    assert(DMA0->TCD[1].SADDR == pop_address);
    assert(DMA0->TCD[1].SOFF == 0U);
    assert(DMA0->TCD[1].ATTR == 0U);
    assert(DMA0->TCD[1].NBYTES_MLNO == 1U);
    assert(DMA0->TCD[1].SLAST == 0);
    assert(DMA0->TCD[1].DADDR == receive_address);
    assert(DMA0->TCD[1].DOFF == 1U);
    assert(DMA0->TCD[1].CITER_ELINKNO == transfer_size);
    assert(DMA0->TCD[1].DLAST_SGA == -((int32_t)transfer_size));
    assert(DMA0->TCD[1].CSR == completion);
    assert(DMA0->TCD[1].BITER_ELINKNO == transfer_size);

    assert((DMAMUX->CHCFG[0] & DMAMUX_CHCFG_SOURCE_MASK) ==
           DMAMUX_CHCFG_SOURCE(kDmaRequestMux0SPI0Tx));
    assert((DMAMUX->CHCFG[1] & DMAMUX_CHCFG_SOURCE_MASK) ==
           DMAMUX_CHCFG_SOURCE(kDmaRequestMux0SPI0Rx));
}

int motor_test_spi_restart(void) {
    MotorSpiTransferBuffers buffers;
    TransferContext transfer = {.seed = 0x30U};

    prepare_calls = 0U;
    prepare_chip_select = 0U;
    motor_spi_initialize(&buffers, prepare_frame, NULL, &transfer);
    assert(prepare_calls == 0U);
    SPI0->RSER = 0U;
    SPI0->MCR |= SPI_MCR_HALT_MASK;

    const uint32_t transmit_address = DMA0->TCD[0].SADDR;
    const uint32_t receive_address = DMA0->TCD[1].DADDR;
    const uint32_t chip_select = GPIOC->PDOR;
    motor_spi_transfer_restart();
    assert(prepare_calls == 0U);
    assert(DMA0->TCD[0].SADDR == transmit_address);
    assert(DMA0->TCD[1].DADDR == receive_address);
    assert(GPIOC->PDOR == chip_select);

    motor_spi_link_active_set(true);
    GPIOC->PDOR = UINT32_C(0x00000003);
    motor_spi_transfer_restart();
    assert(prepare_calls == 1U);
    assert(prepare_chip_select == UINT32_C(0x00000003));
    for (uint32_t index = 0U; index < MOTOR_SPI_TRANSFER_SIZE; ++index) {
        assert(buffers.transmit[index] == (uint8_t)(transfer.seed + index));
    }
    assert(GPIOC->PDOR == UINT32_C(0x00000ffe));
    assert_dma_layout(&buffers);

    transfer.receive_calls = 0U;
    motor_spi_initialize(&buffers, prepare_frame, receive_frame, &transfer);
    motor_spi_link_active_set(true);
    buffers.receive[0] = 1U;
    DMA1_DMA5_IRQHandler();
    buffers.receive[0] = 0U;
    DMA1_DMA5_IRQHandler();
    const uint32_t prepare_count = prepare_calls;
    motor_spi_timeout_service();
    assert(transfer.receive_calls == 2U);
    assert(prepare_calls == prepare_count + 1U);

    return 0;
}
