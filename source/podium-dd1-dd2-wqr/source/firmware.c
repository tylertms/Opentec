#include <stdint.h>
#include <string.h>

#include "fsl_adc16.h"
#include "fsl_clock.h"
#include "fsl_dmamux.h"
#include "fsl_dspi.h"
#include "fsl_edma.h"
#include "fsl_gpio.h"
#include "fsl_i2c.h"
#include "fsl_lptmr.h"
#include "fsl_pit.h"
#include "fsl_port.h"
#include "fsl_smc.h"
#include "fsl_uart.h"
#include "fsl_uart_edma.h"
#include "fsl_wdog.h"
#include "protocol.h"

enum {
    UART_WINDOW_SIZE = 68,
    UART_TRANSMIT_SIZE = 72,
    UART_FRAME_OFFSET = 4,
    UART_FRAME_START = 0x7b,
    UART_FRAME_END = 0x7d,
    UART_SOURCE_CLOCK = 96000000,
    UART_BAUD_RATE = 5000000,
    UART_RESPONSE_GUARD_TICKS = 467,
    UART_RECOVERY_GUARD_TICKS = 3964,

    BUS_CLOCK = 24000000,
    PIT_TICKS_PER_MILLISECOND = 24000,

    UART_TRANSMIT_DMA_CHANNEL = 0,
    UART_RECEIVE_DMA_CHANNEL = 1,
    SPI_TRANSMIT_DMA_CHANNEL = 2,
    SPI_RECEIVE_DMA_CHANNEL = 3,

    UART_RECEIVE_DMA_SOURCE = 4,
    UART_TRANSMIT_DMA_SOURCE = 5,
    SPI_RECEIVE_DMA_SOURCE = 14,
    SPI_TRANSMIT_DMA_SOURCE = 15,

    SPI_RETRY_MILLISECONDS = 2,
    I2C_TIMEOUT_MILLISECONDS = 10,

    SPI_ERROR_INTERRUPTS =
        kDSPI_TxFifoUnderflowInterruptEnable | kDSPI_RxFifoOverflowInterruptEnable,
    SPI_ERROR_FLAGS = kDSPI_TxFifoUnderflowFlag | kDSPI_RxFifoOverflowFlag
};

typedef enum { I2C_IDLE, I2C_PENDING, I2C_SUCCEEDED, I2C_FAILED } i2c_phase;

static wqr_protocol protocol;

static uint8_t uart_receive_window[UART_WINDOW_SIZE] __attribute__((aligned(4)));
static uint8_t uart_transmit_window[UART_TRANSMIT_SIZE] __attribute__((aligned(4)));
static volatile bool uart_receive_ready;
static volatile bool uart_transmit_active;
static bool uart_response_due;
static volatile bool uart_recovery_active;
static uart_edma_handle_t uart_handle;
static edma_handle_t uart_transmit_dma;
static edma_handle_t uart_receive_dma;

static volatile bool spi_transfer_complete;
static volatile bool spi_transfer_failed;
static volatile bool spi_transfer_active;
static volatile bool spi_word_active;
static volatile uint8_t spi_retry_delay;
static const uint8_t *spi_transmit_source;
static uint8_t *spi_receive_destination;
static size_t spi_transfer_length;
static uint16_t spi_transmitted_word;
static uint16_t spi_received_word;
static dspi_master_handle_t spi_word_handle;
static edma_handle_t spi_transmit_dma;
static edma_handle_t spi_receive_dma;

static volatile i2c_phase i2c_state;
static volatile uint8_t i2c_timeout;
static i2c_master_handle_t i2c_handle;

static volatile bool adc_sample_ready;
static volatile uint16_t adc_sample;

static void reset_if_failed(status_t status) {
    if (status != kStatus_Success) {
        NVIC_SystemReset();
    }
}

static void nvic_enable(IRQn_Type interrupt, uint32_t priority) {
    NVIC_SetPriority(interrupt, priority);
    NVIC_EnableIRQ(interrupt);
}

static void configure_clock(void) {
    lptmr_config_t stability_timer;
    sim_clock_config_t clocks = {
        .pllFllSel = 0,
        .er32kSrc = 3,
        .clkdiv1 = SIM_CLKDIV1_OUTDIV1(0) | SIM_CLKDIV1_OUTDIV2(3) | SIM_CLKDIV1_OUTDIV4(3),
    };

    if ((RCM->SRS0 & RCM_SRS0_WAKEUP_MASK) != 0 && (PMC->REGSC & PMC_REGSC_ACKISO_MASK) != 0) {
        PMC->REGSC |= PMC_REGSC_ACKISO_MASK;
    }

    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeHsrun);
    reset_if_failed(SMC_SetPowerModeHsrun(SMC));
    while (SMC_GetPowerModeState(SMC) != kSMC_PowerStateHsrun) {
    }

    CLOCK_SetSimConfig(&clocks);
    OSC->CR = OSC_CR_ERCLKEN_MASK;
    MCG->C2 = (uint8_t)((MCG->C2 & ~MCG_C2_RANGE_MASK) | MCG_C2_RANGE(1));
    reset_if_failed(CLOCK_BootToFeeMode(kMCG_OscselIrc, 6, kMCG_Dmx32Default, kMCG_DrsHigh, NULL));
    MCG->C1 |= MCG_C1_IRCLKEN_MASK;

    LPTMR_GetDefaultConfig(&stability_timer);
    LPTMR_Init(LPTMR0, &stability_timer);
    LPTMR_SetTimerPeriod(LPTMR0, 1);
    LPTMR_StartTimer(LPTMR0);
    while ((LPTMR_GetStatusFlags(LPTMR0) & kLPTMR_TimerCompareFlag) == 0) {
    }
    LPTMR_Deinit(LPTMR0);

    SystemCoreClock = UART_SOURCE_CLOCK;
}

static void configure_pins(void) {
    const gpio_pin_config_t output_low = {kGPIO_DigitalOutput, 0};
    const gpio_pin_config_t output_high = {kGPIO_DigitalOutput, 1};
    uint32_t input_config =
        PORT_PCR_MUX(1) | PORT_PCR_PFE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortC);
    CLOCK_EnableClock(kCLOCK_PortD);
    CLOCK_EnableClock(kCLOCK_PortE);

    PORT_SetPinMux(PORTE, 16, kPORT_MuxAlt3);
    PORT_SetPinMux(PORTE, 17, kPORT_MuxAlt3);

    PORT_SetPinMux(PORTC, 5, kPORT_MuxAlt2);
    PORT_SetPinMux(PORTC, 6, kPORT_MuxAlt2);
    PORT_SetPinMux(PORTC, 7, kPORT_MuxAlt2);

    PORT_SetPinMux(PORTC, 4, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTC, 1, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTC, 2, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTC, 3, kPORT_MuxAsGpio);

    PORTB->PCR[0] = PORT_PCR_MUX(2) | PORT_PCR_ODE_MASK;
    PORTB->PCR[1] = PORT_PCR_MUX(2) | PORT_PCR_ODE_MASK;

    PORTA->PCR[4] = input_config;
    PORTA->PCR[18] = input_config;
    PORTA->PCR[19] = input_config;

    GPIO_PinInit(GPIOC, 1, &output_low);
    GPIO_PinInit(GPIOC, 3, &output_low);
    GPIO_PinInit(GPIOC, 4, &output_high);
}

static void start_uart_guard(uint32_t ticks) {
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_1, ticks + 1);
    PIT_StartTimer(PIT, kPIT_Chnl_1);
}

static void start_uart_recovery(void) {
    UART_TransferAbortReceiveEDMA(UART1, &uart_handle);
    uart_recovery_active = true;
    start_uart_guard(UART_RECOVERY_GUARD_TICKS);
}

static void uart_callback(UART_Type *base, uart_edma_handle_t *handle, status_t status,
                          void *data) {
    (void)base;
    (void)handle;
    (void)data;
    if (status == kStatus_UART_RxIdle) {
        uart_receive_ready = true;
    }
}

static status_t configure_uart_receive(void) {
    uart_transfer_t transfer = {
        .data = uart_receive_window,
        .dataSize = UART_WINDOW_SIZE,
    };
    return UART_ReceiveEDMA(UART1, &uart_handle, &transfer);
}

static void configure_uart(void) {
    uart_config_t config;

    UART_GetDefaultConfig(&config);
    config.baudRate_Bps = UART_BAUD_RATE;
    config.enableTx = true;
    config.enableRx = true;
    UART_Init(UART1, &config, UART_SOURCE_CLOCK);
    UART1->S2 = UART_S2_RXINV_MASK;
    UART1->C3 |= UART_C3_TXINV_MASK;
    UART_EnableInterrupts(UART1, kUART_RxOverrunInterruptEnable | kUART_NoiseErrorInterruptEnable |
                                     kUART_FramingErrorInterruptEnable |
                                     kUART_ParityErrorInterruptEnable);

    EDMA_CreateHandle(&uart_transmit_dma, DMA0, UART_TRANSMIT_DMA_CHANNEL);
    EDMA_CreateHandle(&uart_receive_dma, DMA0, UART_RECEIVE_DMA_CHANNEL);
    DMAMUX_SetSource(DMAMUX, UART_TRANSMIT_DMA_CHANNEL, UART_TRANSMIT_DMA_SOURCE);
    DMAMUX_SetSource(DMAMUX, UART_RECEIVE_DMA_CHANNEL, UART_RECEIVE_DMA_SOURCE);
    DMAMUX_EnableChannel(DMAMUX, UART_TRANSMIT_DMA_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, UART_RECEIVE_DMA_CHANNEL);

    UART_TransferCreateHandleEDMA(UART1, &uart_handle, uart_callback, NULL, &uart_transmit_dma,
                                  &uart_receive_dma);
    reset_if_failed(configure_uart_receive());

    nvic_enable(DMA0_IRQn, 15);
    nvic_enable(DMA1_IRQn, 15);
    nvic_enable(UART1_RX_TX_IRQn, 15);
    nvic_enable(UART1_ERR_IRQn, 15);
}

static void configure_pit(void) {
    pit_config_t config;

    PIT_GetDefaultConfig(&config);
    PIT_Init(PIT, &config);
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, PIT_TICKS_PER_MILLISECOND + 1);
    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
    PIT_EnableInterrupts(PIT, kPIT_Chnl_1, kPIT_TimerInterruptEnable);
    PIT_StartTimer(PIT, kPIT_Chnl_0);
    nvic_enable(PIT0_IRQn, 11);
    nvic_enable(PIT1_IRQn, 13);
}

static void configure_adc(void) {
    adc16_config_t config;

    ADC16_GetDefaultConfig(&config);
    config.clockSource = kADC16_ClockSourceAlt0;
    config.clockDivider = kADC16_ClockDivider2;
    config.resolution = kADC16_ResolutionSE12Bit;
    config.hardwareAverageMode = kADC16_HardwareAverageCount32;
    ADC16_Init(ADC0, &config);
    reset_if_failed(ADC16_DoAutoCalibration(ADC0));
    nvic_enable(ADC0_IRQn, 9);
}

static void finish_spi(status_t status) {
    GPIO_PinWrite(GPIOC, 4, 1);
    spi_retry_delay = 0;
    spi_transfer_failed = status != kStatus_Success;
    spi_transfer_complete = true;
}

static void spi_word_callback(SPI_Type *base, dspi_master_handle_t *handle, status_t status,
                              void *data) {
    (void)handle;
    (void)data;
    DSPI_DisableInterrupts(base, SPI_ERROR_INTERRUPTS);
    finish_spi(status);
}

static void set_spi_format(unsigned int bits, dspi_clock_phase_t phase) {
    DSPI_StopTransfer(SPI0);
    SPI0->CTAR[0] = (SPI0->CTAR[0] & ~(SPI_CTAR_FMSZ_MASK | SPI_CTAR_CPHA_MASK)) |
                    SPI_CTAR_FMSZ(bits - 1) |
                    (phase == kDSPI_ClockPhaseSecondEdge ? SPI_CTAR_CPHA_MASK : 0);
    DSPI_ClearStatusFlags(SPI0, kDSPI_AllStatusFlag);
    DSPI_StartTransfer(SPI0);
}

static void configure_spi(void) {
    dspi_master_config_t config;

    DSPI_MasterGetDefaultConfig(&config);
    config.whichCtar = kDSPI_Ctar0;
    config.ctarConfig.bitsPerFrame = 8;
    config.ctarConfig.cpha = kDSPI_ClockPhaseSecondEdge;
    config.whichPcs = kDSPI_Pcs0;
    config.pcsActiveHighOrLow = kDSPI_PcsActiveLow;
    DSPI_MasterInit(SPI0, &config, BUS_CLOCK);
    SPI0->CTAR[0] = UINT32_C(0x3a514204);

    EDMA_CreateHandle(&spi_transmit_dma, DMA0, SPI_TRANSMIT_DMA_CHANNEL);
    EDMA_CreateHandle(&spi_receive_dma, DMA0, SPI_RECEIVE_DMA_CHANNEL);
    DMAMUX_SetSource(DMAMUX, SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE);
    DMAMUX_SetSource(DMAMUX, SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE);
    DMAMUX_EnableChannel(DMAMUX, SPI_TRANSMIT_DMA_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, SPI_RECEIVE_DMA_CHANNEL);

    DSPI_MasterTransferCreateHandle(SPI0, &spi_word_handle, spi_word_callback, NULL);

    nvic_enable(DMA2_IRQn, 14);
    nvic_enable(DMA3_IRQn, 14);
    nvic_enable(SPI0_IRQn, 14);
}

static void start_spi_dma(void) {
    edma_transfer_config_t receive;
    edma_transfer_config_t transmit;

    EDMA_AbortTransfer(&spi_receive_dma);
    EDMA_AbortTransfer(&spi_transmit_dma);
    set_spi_format(8, kDSPI_ClockPhaseSecondEdge);

    EDMA_PrepareTransfer(&receive, (void *)(uintptr_t)DSPI_GetRxRegisterAddress(SPI0), 1,
                         spi_receive_destination, 1, 1, spi_transfer_length,
                         kEDMA_PeripheralToMemory);
    EDMA_PrepareTransfer(&transmit, (void *)(uintptr_t)spi_transmit_source, 1,
                         (void *)(uintptr_t)DSPI_MasterGetTxRegisterAddress(SPI0), 1, 1,
                         spi_transfer_length, kEDMA_MemoryToPeripheral);
    EDMA_SubmitTransfer(&spi_receive_dma, &receive);
    EDMA_SubmitTransfer(&spi_transmit_dma, &transmit);

    spi_transfer_complete = false;
    spi_transfer_failed = false;
    spi_retry_delay = SPI_RETRY_MILLISECONDS;
    GPIO_PinWrite(GPIOC, 4, 0);

    DSPI_EnableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
    EDMA_StartTransfer(&spi_receive_dma);
    EDMA_StartTransfer(&spi_transmit_dma);
}

static wqr_io_result io_spi_transfer(void *context, const uint8_t *transmit, uint8_t *receive,
                                     size_t length) {
    (void)context;
    if (spi_transfer_active) {
        if (!spi_transfer_complete) {
            return WQR_IO_PENDING;
        }
        spi_transfer_complete = false;
        spi_transfer_active = false;
        return spi_transfer_failed ? WQR_IO_FAILED : WQR_IO_SUCCEEDED;
    }

    spi_transmit_source = transmit;
    spi_receive_destination = receive;
    spi_transfer_length = length;
    spi_word_active = false;
    spi_transfer_active = true;
    start_spi_dma();
    return WQR_IO_PENDING;
}

static wqr_io_result io_spi_word(void *context, uint16_t transmit, uint16_t *receive) {
    dspi_transfer_t transfer;

    (void)context;
    if (spi_transfer_active) {
        if (!spi_transfer_complete) {
            return WQR_IO_PENDING;
        }
        spi_transfer_complete = false;
        spi_transfer_active = false;
        *receive = spi_received_word;
        return spi_transfer_failed ? WQR_IO_FAILED : WQR_IO_SUCCEEDED;
    }

    spi_transmitted_word = transmit;
    transfer.txData = (const uint8_t *)&spi_transmitted_word;
    transfer.rxData = (uint8_t *)&spi_received_word;
    transfer.dataSize = sizeof(spi_transmitted_word);
    transfer.configFlags = kDSPI_MasterCtar0;

    set_spi_format(16, kDSPI_ClockPhaseFirstEdge);
    spi_transfer_complete = false;
    spi_transfer_failed = false;
    spi_transfer_active = true;
    spi_word_active = true;
    spi_retry_delay = 0;
    GPIO_PinWrite(GPIOC, 4, 0);

    if (DSPI_MasterTransferNonBlocking(SPI0, &spi_word_handle, &transfer) == kStatus_Success) {
        DSPI_EnableInterrupts(SPI0, SPI_ERROR_INTERRUPTS);
    } else {
        finish_spi(kStatus_Fail);
    }
    return WQR_IO_PENDING;
}

static void i2c_callback(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *data) {
    (void)base;
    (void)handle;
    (void)data;

    i2c_timeout = 0;
    I2C0->FLT |= I2C_FLT_SSIE_MASK;
    i2c_state = status == kStatus_Success ? I2C_SUCCEEDED : I2C_FAILED;
}

static wqr_io_result i2c_result(void) {
    if (i2c_state == I2C_SUCCEEDED) {
        i2c_state = I2C_IDLE;
        return WQR_IO_SUCCEEDED;
    }
    if (i2c_state == I2C_FAILED) {
        i2c_state = I2C_IDLE;
        return WQR_IO_FAILED;
    }

    return WQR_IO_PENDING;
}

static wqr_io_result start_i2c(i2c_master_transfer_t *transfer) {
    I2C0->FLT &= (uint8_t)~I2C_FLT_SSIE_MASK;
    i2c_timeout = I2C_TIMEOUT_MILLISECONDS;
    i2c_state = I2C_PENDING;

    if (I2C_MasterTransferNonBlocking(I2C0, &i2c_handle, transfer) != kStatus_Success) {
        i2c_timeout = 0;
        i2c_state = I2C_FAILED;
        I2C0->FLT |= I2C_FLT_SSIE_MASK;
    }

    return WQR_IO_PENDING;
}

static wqr_io_result io_i2c_write(void *context, uint8_t address, const uint8_t *data,
                                  size_t length) {
    i2c_master_transfer_t transfer = {
        .flags = kI2C_TransferDefaultFlag,
        .slaveAddress = address >> 1,
        .direction = kI2C_Write,
        .data = (uint8_t *)(uintptr_t)data,
        .dataSize = length,
    };
    wqr_io_result result;

    (void)context;
    result = i2c_result();
    if (result != WQR_IO_PENDING || i2c_state != I2C_IDLE) {
        return result;
    }

    return start_i2c(&transfer);
}

static wqr_io_result io_i2c_read(void *context, uint8_t address, uint8_t command, uint8_t *data,
                                 size_t length) {
    i2c_master_transfer_t transfer = {
        .flags = kI2C_TransferDefaultFlag,
        .slaveAddress = address >> 1,
        .direction = kI2C_Read,
        .subaddress = command,
        .subaddressSize = 1,
        .data = data,
        .dataSize = length,
    };
    wqr_io_result result;

    (void)context;
    result = i2c_result();
    if (result != WQR_IO_PENDING || i2c_state != I2C_IDLE) {
        return result;
    }
    if (length == 0) {
        return WQR_IO_SUCCEEDED;
    }

    return start_i2c(&transfer);
}

static void configure_i2c(void) {
    i2c_master_config_t config;

    I2C_MasterGetDefaultConfig(&config);
    config.baudRate_Bps = 1200000;
    config.glitchFilterWidth = 10;
    I2C_MasterInit(I2C0, &config, BUS_CLOCK);
    I2C0->FLT |= I2C_FLT_SSIE_MASK;
    I2C_MasterTransferCreateHandle(I2C0, &i2c_handle, i2c_callback, NULL);
    i2c_state = I2C_IDLE;

    nvic_enable(I2C0_IRQn, 10);
}

static uint8_t io_read_inputs(void *context) {
    (void)context;
    return (uint8_t)((GPIO_PinRead(GPIOA, 4) == 0 ? 1 : 0) |
                     (GPIO_PinRead(GPIOA, 18) == 0 ? 2 : 0) |
                     (GPIO_PinRead(GPIOA, 19) == 0 ? 4 : 0));
}

static bool io_transfer_ready(void *context) {
    (void)context;
    return GPIO_PinRead(GPIOC, 2) == 0;
}

static void io_set_transfer_control(void *context, bool asserted) {
    (void)context;
    GPIO_PinWrite(GPIOC, 3, asserted ? 1 : 0);
}

static void io_request_reset(void *context) {
    (void)context;
    NVIC_SystemReset();
}

static void configure_watchdog(void) {
    wdog_config_t config;

    WDOG_GetDefaultConfig(&config);
    config.workMode.enableStop = true;
    config.enableInterrupt = true;
    config.timeoutValue = 500;
    WDOG_Init(WDOG, &config);
}

static void prepare_transmit_window(void) {
    memset(uart_transmit_window, 0, sizeof(uart_transmit_window));
    memset(uart_transmit_window, 0xf0, UART_FRAME_OFFSET);
    memset(uart_transmit_window + UART_TRANSMIT_SIZE - UART_FRAME_OFFSET, 0xf0, UART_FRAME_OFFSET);
}

void DMA0_IRQHandler(void) {
    uart_transmit_active = false;
    start_uart_guard(UART_RESPONSE_GUARD_TICKS);
    EDMA_HandleIRQ(&uart_transmit_dma);
}

void DMA1_IRQHandler(void) { EDMA_HandleIRQ(&uart_receive_dma); }

void DMA2_IRQHandler(void) {
    EDMA_ClearChannelStatusFlags(DMA0, SPI_TRANSMIT_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_InterruptFlag);
}

void DMA3_IRQHandler(void) {
    EDMA_ClearChannelStatusFlags(DMA0, SPI_RECEIVE_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_InterruptFlag);
    DSPI_DisableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
    finish_spi(kStatus_Success);
}

void SPI0_IRQHandler(void) {
    uint32_t errors = DSPI_GetStatusFlags(SPI0) & SPI_ERROR_FLAGS;

    if (errors == 0) {
        DSPI_MasterTransferHandleIRQ(SPI0, &spi_word_handle);
        return;
    }
    DSPI_MasterTransferAbort(SPI0, &spi_word_handle);
    DSPI_DisableInterrupts(SPI0, SPI_ERROR_INTERRUPTS);
    DSPI_ClearStatusFlags(SPI0, errors);
    finish_spi(kStatus_DSPI_Error);
}

void I2C0_IRQHandler(void) {
    uint8_t detection = I2C0->FLT & (I2C_FLT_STARTF_MASK | I2C_FLT_STOPF_MASK);

    I2C0->FLT |= detection;
    if ((I2C0->S & I2C_S_IICIF_MASK) == 0) {
        return;
    }
    I2C_MasterTransferHandleIRQ(I2C0, &i2c_handle);
}

void UART1_ERR_DriverIRQHandler(void) {
    uint8_t status = UART1->S1;

    if ((status & (UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK)) != 0) {
        (void)UART1->D;
    }
}

static void update_i2c_timeout(void) {
    if (i2c_timeout == 0) {
        return;
    }
    --i2c_timeout;
    if (i2c_timeout == 0) {
        I2C_MasterTransferAbort(I2C0, &i2c_handle);
        I2C0->FLT |= I2C_FLT_SSIE_MASK;
        i2c_state = I2C_FAILED;
    }
}

static void update_spi_retry(void) {
    if (!spi_transfer_active || spi_word_active || spi_transfer_complete || spi_retry_delay == 0) {
        return;
    }
    if (--spi_retry_delay == 0) {
        start_spi_dma();
    }
}

void PIT0_IRQHandler(void) {
    static uint8_t adc_period;

    wqr_protocol_tick(&protocol);
    update_spi_retry();
    update_i2c_timeout();
    if (++adc_period == 100) {
        const adc16_channel_config_t channel = {
            .channelNumber = 23,
            .enableInterruptOnConversionCompleted = true,
        };
        adc_period = 0;
        ADC16_SetChannelConfig(ADC0, 0, &channel);
    }
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
}

void PIT1_IRQHandler(void) {
    bool response_sent = !uart_recovery_active;

    PIT_ClearStatusFlags(PIT, kPIT_Chnl_1, kPIT_TimerFlag);
    PIT_StopTimer(PIT, kPIT_Chnl_1);
    uart_recovery_active = false;
    if (configure_uart_receive() != kStatus_Success) {
        start_uart_recovery();
    }
    if (response_sent) {
        wqr_protocol_response_sent(&protocol);
    }
}

void ADC0_IRQHandler(void) {
    adc_sample = (uint16_t)ADC16_GetChannelConversionValue(ADC0, 0);
    adc_sample_ready = true;
}

void WDOG_EWM_IRQHandler(void) {}

static void process_uart_frame(void) {
    size_t offset;

    for (offset = 0; offset <= 4; ++offset) {
        if (uart_receive_window[offset] == UART_FRAME_START) {
            break;
        }
    }
    if (offset > 4 || uart_receive_window[offset + WQR_FRAME_SIZE - 1] != UART_FRAME_END) {
        offset = 0;
        start_uart_recovery();
    }

    wqr_protocol_receive(&protocol, uart_receive_window + offset);
    uart_response_due = true;
}

static void start_uart_response(void) {
    uart_transfer_t transfer;

    if (!uart_response_due || uart_transmit_active || uart_recovery_active ||
        !wqr_protocol_response(&protocol, uart_transmit_window + UART_FRAME_OFFSET)) {
        return;
    }
    uart_transmit_active = true;
    transfer.data = uart_transmit_window;
    transfer.dataSize = UART_TRANSMIT_SIZE;

    if (UART_SendEDMA(UART1, &uart_handle, &transfer) != kStatus_Success) {
        uart_transmit_active = false;
        return;
    }
    uart_response_due = false;
}

void firmware_main(void) {
    const wqr_io io = {
        .spi_transfer = io_spi_transfer,
        .spi_word = io_spi_word,
        .i2c_write = io_i2c_write,
        .i2c_read = io_i2c_read,
        .read_inputs = io_read_inputs,
        .transfer_ready = io_transfer_ready,
        .set_transfer_control = io_set_transfer_control,
        .request_reset = io_request_reset,
    };
    edma_config_t dma;

    __disable_irq();
    configure_watchdog();
    configure_clock();
    configure_pins();
    prepare_transmit_window();

    EDMA_GetDefaultConfig(&dma);
    EDMA_Init(DMA0, &dma);
    DMAMUX_Init(DMAMUX);

    configure_uart();
    configure_pit();
    configure_adc();
    configure_spi();
    configure_i2c();
    wqr_protocol_init(&protocol, &io);
    __enable_irq();

    for (;;) {
        wqr_protocol_poll(&protocol);
        if (uart_receive_ready) {
            uart_receive_ready = false;
            process_uart_frame();
        }

        if (adc_sample_ready) {
            adc_sample_ready = false;
            wqr_protocol_set_sensor_sample(&protocol, adc_sample);
        }

        start_uart_response();
        WDOG_Refresh(WDOG);
    }
}
