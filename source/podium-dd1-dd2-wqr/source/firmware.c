#include <fsl_adc16.h>
#include <fsl_clock.h>
#include <fsl_dmamux.h>
#include <fsl_dspi.h>
#include <fsl_edma.h>
#include <fsl_gpio.h>
#include <fsl_i2c.h>
#include <fsl_lptmr.h>
#include <fsl_pit.h>
#include <fsl_port.h>
#include <fsl_smc.h>
#include <fsl_uart.h>
#include <fsl_uart_edma.h>
#include <fsl_wdog.h>
#include <stdint.h>
#include <string.h>

#include "protocol.h"

/** @brief WQR firmware timing, peripheral, DMA, and watchdog constants. */
enum {
    UART_WINDOW_SIZE = 68,            /**< Number of bytes in the UART receive window. */
    UART_TRANSMIT_SIZE = 72,          /**< Number of bytes in the guarded UART transmit window. */
    UART_FRAME_OFFSET = 4,            /**< Number of leading guard bytes in a UART frame window. */
    UART_FRAME_START = 0x7b,          /**< UART frame start marker. */
    UART_FRAME_END = 0x7d,            /**< UART frame end marker. */
    UART_SOURCE_CLOCK = 96000000,     /**< UART source clock frequency in hertz. */
    UART_BAUD_RATE = 5000000,         /**< WQR UART baud rate in bits per second. */
    UART_RESPONSE_GUARD_TICKS = 467,  /**< PIT ticks reserved after a UART response. */
    UART_RECOVERY_GUARD_TICKS = 3964, /**< PIT ticks reserved for UART recovery. */

    BUS_CLOCK = 24000000,              /**< Peripheral bus clock frequency in hertz. */
    PIT_TICKS_PER_MILLISECOND = 24000, /**< PIT ticks in one millisecond at the bus clock. */

    UART_TRANSMIT_DMA_CHANNEL = 0, /**< DMA channel assigned to UART transmission. */
    UART_RECEIVE_DMA_CHANNEL = 1,  /**< DMA channel assigned to UART reception. */
    SPI_TRANSMIT_DMA_CHANNEL = 2,  /**< DMA channel assigned to SPI transmission. */
    SPI_RECEIVE_DMA_CHANNEL = 3,   /**< DMA channel assigned to SPI reception. */

    UART_RECEIVE_DMA_SOURCE = 4,  /**< DMAMUX source assigned to UART reception. */
    UART_TRANSMIT_DMA_SOURCE = 5, /**< DMAMUX source assigned to UART transmission. */
    SPI_RECEIVE_DMA_SOURCE = 14,  /**< DMAMUX source assigned to SPI reception. */
    SPI_TRANSMIT_DMA_SOURCE = 15, /**< DMAMUX source assigned to SPI transmission. */

    SPI_RETRY_TICKS = 3, /**< Milliseconds to wait before retrying a primary SPI transfer. */
    I2C_TIMEOUT_MILLISECONDS = 10, /**< Timeout applied to one asynchronous I2C transfer. */

    WATCHDOG_UNLOCK_FIRST = 0xc520,  /**< First watchdog unlock halfword. */
    WATCHDOG_UNLOCK_SECOND = 0xd928, /**< Second watchdog unlock halfword. */
    WATCHDOG_CONTROL_SET = 0x40d5,   /**< Watchdog control bits written during setup. */
    WATCHDOG_CONTROL_CLEAR = 0x042a, /**< Watchdog control bits cleared during setup. */
    WATCHDOG_TIMEOUT_LOW = 500,      /**< Low half of the watchdog timeout value. */

    SPI_ERROR_INTERRUPTS =
        kDSPI_TxFifoUnderflowInterruptEnable |
        kDSPI_RxFifoOverflowInterruptEnable, /**< SPI FIFO error interrupt mask. */
    SPI_ERROR_FLAGS =
        kDSPI_TxFifoUnderflowFlag | kDSPI_RxFifoOverflowFlag /**< SPI FIFO error status mask. */
};

/**
 * @brief Phase of the asynchronous I2C transfer state machine.
 *
 * The phase records whether a transfer can start, is awaiting an interrupt, or has a terminal
 * result ready for the protocol service.
 */
typedef enum {
    I2C_IDLE,      /**< No I2C transfer is active. */
    I2C_PENDING,   /**< An I2C transfer is awaiting completion. */
    I2C_SUCCEEDED, /**< The active I2C transfer completed successfully. */
    I2C_FAILED     /**< The last I2C transfer failed and recovery has completed. */
} i2c_phase;

/** @brief Protocol endpoint state shared by the firmware main loop and interrupts. */
static wqr_protocol protocol;

/** @brief Aligned UART receive window containing one guarded frame. */
static uint8_t uart_receive_window[UART_WINDOW_SIZE] __attribute__((aligned(4)));
/** @brief Aligned UART transmit window containing guards and one response frame. */
static uint8_t uart_transmit_window[UART_TRANSMIT_SIZE] __attribute__((aligned(4)));
/** @brief Set by the UART receive path when a complete window is ready. */
static volatile bool uart_receive_ready;
/** @brief Set while a UART response DMA transfer is active. */
static volatile bool uart_transmit_active;
/** @brief Set after UART response DMA completes until the guard expires. */
static volatile bool uart_response_sent_pending;
/** @brief Set when the main loop has a protocol response waiting to transmit. */
static volatile bool uart_response_due;
/** @brief Set while the UART receive path is in guarded recovery. */
static volatile bool uart_recovery_active;
/** @brief SDK state for the shared UART eDMA transfer callbacks. */
static uart_edma_handle_t uart_handle;
/** @brief eDMA handle for UART transmission. */
static edma_handle_t uart_transmit_dma;
/** @brief eDMA handle for UART reception. */
static edma_handle_t uart_receive_dma;

/** @brief Set when the active SPI transfer has reached a terminal state. */
static volatile bool spi_transfer_complete;
/** @brief Set when the active SPI transfer failed. */
static volatile bool spi_transfer_failed;
/** @brief Set while any SPI transfer is active. */
static volatile bool spi_transfer_active;
/** @brief Set while or after the alternate word-oriented SPI path owns the SPI module. */
static volatile bool spi_word_active;
/** @brief Set when the primary SPI retry delay has expired. */
static volatile bool spi_retry_pending;
/** @brief Remaining milliseconds before retrying a primary SPI transfer. */
static volatile uint8_t spi_retry_delay;
/** @brief Stable transmit storage for the active primary SPI DMA transfer. */
static uint8_t spi_transmit_buffer[WQR_SPI_TRANSFER_SIZE];
/** @brief Stable receive storage for the active primary SPI DMA transfer. */
static uint8_t spi_receive_buffer[WQR_SPI_TRANSFER_SIZE];
/** @brief Number of bytes in the active primary SPI transfer. */
static size_t spi_transfer_length;
/** @brief Transmit word retained for the active alternate SPI transfer. */
static uint16_t spi_transmitted_word;
/** @brief Receive word storage for the active alternate SPI transfer. */
static uint16_t spi_received_word;
/** @brief SDK state for interrupt-driven alternate SPI transfers. */
static dspi_master_handle_t spi_word_handle;
/** @brief eDMA handle for primary SPI transmission. */
static edma_handle_t spi_transmit_dma;
/** @brief eDMA handle for primary SPI reception. */
static edma_handle_t spi_receive_dma;

/** @brief Current asynchronous I2C transfer phase. */
static volatile i2c_phase i2c_state;
/** @brief Remaining milliseconds before the active I2C transfer times out. */
static volatile uint8_t i2c_timeout;
/** @brief Set when the main loop must reinitialize the I2C peripheral. */
static volatile bool i2c_recovery_pending;
/** @brief Sink byte used for zero-length I2C reads. */
static uint8_t i2c_discard;
/** @brief SDK state for the nonblocking I2C transfer callback. */
static i2c_master_handle_t i2c_handle;

/** @brief Set by the ADC interrupt when a new sensor sample is available. */
static volatile bool adc_sample_ready;
/** @brief Latest ADC sample published by the ADC interrupt. */
static volatile uint16_t adc_sample;
/** @brief Set when the protocol has requested a deferred system reset. */
static volatile bool reset_pending;

static void restore_uart_registers(void);
static status_t configure_uart_receive(void);
static status_t rearm_uart_receive(void);

/**
 * @brief Resets the device after a failed mandatory initialization step.
 *
 * Leaves successful SDK results unchanged and requests an immediate system reset for every failure
 * status.
 *
 * @param[in] status SDK operation result to check.
 */
static void reset_if_failed(status_t status) {
    if (status != kStatus_Success) {
        NVIC_SystemReset();
    }
}

/**
 * @brief Configures and enables one NVIC interrupt.
 *
 * Applies the requested priority before enabling delivery.
 *
 * @param[in] interrupt Interrupt number to enable.
 * @param[in] priority NVIC priority value.
 */
static void nvic_enable(IRQn_Type interrupt, uint32_t priority) {
    NVIC_SetPriority(interrupt, priority);
    NVIC_EnableIRQ(interrupt);
}

/**
 * @brief Programs one DMA multiplexer channel.
 *
 * Replaces the channel source and sets its enable bit to the requested state in one write.
 *
 * @param[in] channel DMA multiplexer channel index.
 * @param[in] source Peripheral request source.
 * @param[in] enabled True to enable request routing.
 */
static void configure_dmamux_channel(uint32_t channel, uint32_t source, bool enabled) {
    DMAMUX->CHCFG[channel] =
        (uint8_t)(DMAMUX_CHCFG_SOURCE(source) | (enabled ? DMAMUX_CHCFG_ENBL_MASK : 0));
}

/**
 * @brief Updates selected fields in one port control register.
 *
 * Preserves unmasked configuration, applies the requested field value, and clears a stale
 * interrupt-status flag through its write-one-to-clear bit.
 *
 * @param[in,out] port Port register block containing the pin.
 * @param[in] pin Port pin index.
 * @param[in] mask Configuration fields to replace.
 * @param[in] value New configuration field values.
 */
static void configure_pin(PORT_Type *port, uint32_t pin, uint32_t mask, uint32_t value) {
    port->PCR[pin] = (port->PCR[pin] & ~mask) | value | PORT_PCR_ISF_MASK;
}

/**
 * @brief Configures the official WQR power and clock state.
 *
 * Acknowledges wake isolation, enters high-speed run mode, normalizes MCG and oscillator state,
 * boots FEE mode, waits through the LPTMR stabilization interval, and publishes the 96 MHz core
 * clock.
 */
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
    MCG->SC = 0;
    MCG->C2 = (uint8_t)((MCG->C2 & (uint8_t)~UINT8_C(0x60)) | UINT8_C(0x20));
    OSC->CR = OSC_CR_ERCLKEN_MASK;
    reset_if_failed(CLOCK_BootToFeeMode(kMCG_OscselIrc, 6, kMCG_Dmx32Default, kMCG_DrsHigh, NULL));
    MCG->C6 = 0;
    MCG->C1 |= MCG_C1_IRCLKEN_MASK;

    LPTMR_GetDefaultConfig(&stability_timer);
    LPTMR_Init(LPTMR0, &stability_timer);
    LPTMR_SetTimerPeriod(LPTMR0, 1);
    LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
    LPTMR_StartTimer(LPTMR0);
    while ((LPTMR_GetStatusFlags(LPTMR0) & kLPTMR_TimerCompareFlag) == 0) {
    }
    LPTMR_Deinit(LPTMR0);

    SystemCoreClock = UART_SOURCE_CLOCK;
}

/**
 * @brief Configures all WQR peripheral and handshake pins.
 *
 * Enables the five port clocks, assigns UART, SPI, I2C, GPIO, and filtered input multiplexing, and
 * establishes safe initial GPIO directions and output levels.
 */
static void configure_pins(void) {
    const gpio_pin_config_t input = {kGPIO_DigitalInput, 0};
    const gpio_pin_config_t output_low = {kGPIO_DigitalOutput, 0};
    const gpio_pin_config_t output_high = {kGPIO_DigitalOutput, 1};
    uint32_t input_config =
        PORT_PCR_MUX(1) | PORT_PCR_PFE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortC);
    CLOCK_EnableClock(kCLOCK_PortD);
    CLOCK_EnableClock(kCLOCK_PortE);

    configure_pin(PORTE, 16, PORT_PCR_MUX_MASK, PORT_PCR_MUX(3));
    configure_pin(PORTE, 17, PORT_PCR_MUX_MASK, PORT_PCR_MUX(3));

    configure_pin(PORTC, 5, PORT_PCR_MUX_MASK, PORT_PCR_MUX(2));
    configure_pin(PORTC, 6, PORT_PCR_MUX_MASK, PORT_PCR_MUX(2));
    configure_pin(PORTC, 7, PORT_PCR_MUX_MASK, PORT_PCR_MUX(2));

    configure_pin(PORTC, 4, PORT_PCR_MUX_MASK, PORT_PCR_MUX(1));
    configure_pin(PORTC, 1, PORT_PCR_MUX_MASK, PORT_PCR_MUX(1));
    configure_pin(PORTC, 2, PORT_PCR_MUX_MASK, PORT_PCR_MUX(1));
    configure_pin(PORTC, 3, PORT_PCR_MUX_MASK, PORT_PCR_MUX(1));

    configure_pin(PORTB, 0, PORT_PCR_MUX_MASK | PORT_PCR_ODE_MASK,
                  PORT_PCR_MUX(2) | PORT_PCR_ODE_MASK);
    configure_pin(PORTB, 1, PORT_PCR_MUX_MASK | PORT_PCR_ODE_MASK,
                  PORT_PCR_MUX(2) | PORT_PCR_ODE_MASK);

    configure_pin(PORTA, 4,
                  PORT_PCR_MUX_MASK | PORT_PCR_PFE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK,
                  input_config);
    configure_pin(PORTA, 18,
                  PORT_PCR_MUX_MASK | PORT_PCR_PFE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK,
                  input_config);
    configure_pin(PORTA, 19,
                  PORT_PCR_MUX_MASK | PORT_PCR_PFE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK,
                  input_config);

    GPIO_PinInit(GPIOA, 4, &input);
    GPIO_PinInit(GPIOA, 18, &input);
    GPIO_PinInit(GPIOA, 19, &input);
    GPIO_PinInit(GPIOC, 2, &input);
    GPIO_PinInit(GPIOC, 1, &output_low);
    GPIO_PinInit(GPIOC, 3, &output_low);
    GPIO_PinInit(GPIOC, 4, &output_high);
}

/**
 * @brief Starts the one-shot UART guard timer.
 *
 * Stops and clears PIT channel 1 before loading the requested delay and restarting the channel.
 *
 * @param[in] ticks Guard delay in bus-clock ticks before the PIT load adjustment.
 */
static void start_uart_guard(uint32_t ticks) {
    PIT_StopTimer(PIT, kPIT_Chnl_1);
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_1, kPIT_TimerFlag);
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_1, ticks + 1);
    PIT_StartTimer(PIT, kPIT_Chnl_1);
}

/**
 * @brief Restarts UART reception before entering guarded recovery.
 *
 * Aborts the active receive descriptor, rebuilds the official receive descriptor immediately, marks
 * recovery active, and schedules the longer recovery guard. A failed rearm leaves the descriptor
 * rewind in the official state so the guard expiry can retry it.
 */
static void start_uart_recovery(void) {
    status_t status;

    UART_TransferAbortReceiveEDMA(UART1, &uart_handle);
    uart_recovery_active = true;
    status = rearm_uart_receive();
    if (status != kStatus_Success) {
        DMA0->TCD[UART_RECEIVE_DMA_CHANNEL].DLAST_SGA =
            (uint32_t)-(int32_t)UART_WINDOW_SIZE;
        restore_uart_registers();
    }
    start_uart_guard(UART_RECOVERY_GUARD_TICKS);
}

/**
 * @brief Publishes completion of one UART receive window.
 *
 * On receive-idle completion, restores the descriptor rewind and UART register contract before a
 * memory barrier publishes the ready flag to the main loop.
 *
 * @param[in] base UART instance supplied by the SDK callback.
 * @param[in] handle UART eDMA handle supplied by the SDK callback.
 * @param[in] status Completed UART transfer status.
 * @param[in] data Callback context supplied during handle creation.
 */
static void uart_callback(UART_Type *base, uart_edma_handle_t *handle, status_t status,
                          void *data) {
    (void)base;
    (void)handle;
    (void)data;
    if (status == kStatus_UART_RxIdle) {
        DMA0->TCD[UART_RECEIVE_DMA_CHANNEL].DLAST_SGA = (uint32_t)-(int32_t)UART_WINDOW_SIZE;
        restore_uart_registers();
        __DMB();
        uart_receive_ready = true;
    }
}

/**
 * @brief Restores the official persistent UART register state.
 *
 * Reapplies inversion, receiver and transmitter enablement, error controls, DMA selection, and the
 * disabled receive FIFO watermark after SDK operations alter them.
 */
static void restore_uart_registers(void) {
    UART1->C1 = 0;
    UART1->C2 = UINT8_C(0xac);
    UART1->S2 = UART_S2_RXINV_MASK;
    UART1->C3 = UINT8_C(0x1f);
    UART1->C5 = UINT8_C(0xa0);
    UART1->RWFIFO = 0;
}

/**
 * @brief Clears latched UART receive error conditions.
 *
 * Reads the data register once for each asserted parity, framing, noise, or overrun status bit so
 * hardware can release the corresponding error state.
 */
static void drain_uart_errors(void) {
    if ((UART1->S1 & UART_S1_PF_MASK) != 0) {
        (void)UART1->D;
    }
    if ((UART1->S1 & UART_S1_FE_MASK) != 0) {
        (void)UART1->D;
    }
    if ((UART1->S1 & UART_S1_NF_MASK) != 0) {
        (void)UART1->D;
    }
    if ((UART1->S1 & UART_S1_OR_MASK) != 0) {
        (void)UART1->D;
    }
}

/**
 * @brief Arms one 68-byte UART receive window through eDMA.
 *
 * Disables routing while clearing the destination and submitting the descriptor, restores the
 * official source and destination offsets, enables routing only after setup, and reapplies UART
 * registers.
 *
 * @return SDK status from receive submission.
 */
static status_t configure_uart_receive(void) {
    uart_transfer_t transfer = {
        .data = uart_receive_window,
        .dataSize = UART_WINDOW_SIZE,
    };
    status_t status;

    SIM->SCGC7 |= SIM_SCGC7_DMA_MASK;
    SIM->SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
    configure_dmamux_channel(UART_RECEIVE_DMA_CHANNEL, UART_RECEIVE_DMA_SOURCE, false);
    EDMA_ClearChannelStatusFlags(DMA0, UART_RECEIVE_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    memset(uart_receive_window, 0, sizeof(uart_receive_window));
    status = UART_ReceiveEDMA(UART1, &uart_handle, &transfer);
    if (status == kStatus_Success) {
        DMA0->TCD[UART_RECEIVE_DMA_CHANNEL].SLAST = 0;
        DMA0->TCD[UART_RECEIVE_DMA_CHANNEL].DLAST_SGA = (uint32_t)-(int32_t)UART_WINDOW_SIZE;
        EDMA_EnableChannelRequest(DMA0, UART_RECEIVE_DMA_CHANNEL);
        configure_dmamux_channel(UART_RECEIVE_DMA_CHANNEL, UART_RECEIVE_DMA_SOURCE, true);
    }
    restore_uart_registers();
    return status;
}

/**
 * @brief Arms UART reception with one retry for a busy eDMA handle.
 *
 * Submits the receive descriptor and retries after an SDK busy result so recovery and guard expiry
 * share the same receive-path contract.
 *
 * @return SDK status from the final receive submission.
 */
static status_t rearm_uart_receive(void) {
    status_t status = configure_uart_receive();

    if (status == kStatus_UART_RxBusy) {
        UART_TransferAbortReceiveEDMA(UART1, &uart_handle);
        status = configure_uart_receive();
    }
    return status;
}

/**
 * @brief Initializes the WQR UART and both eDMA channels.
 *
 * Configures the 5 Mbaud inverted link, prebuilds the official transmit descriptor, arms receive
 * DMA, enables error reporting, and establishes the required interrupt priorities.
 */
static void configure_uart(void) {
    edma_transfer_config_t transmit;
    uart_config_t config;

    UART_GetDefaultConfig(&config);
    config.baudRate_Bps = UART_BAUD_RATE;
    config.enableTx = true;
    config.enableRx = true;
    UART_Init(UART1, &config, UART_SOURCE_CLOCK);
    UART1->S2 = UART_S2_RXINV_MASK;
    UART1->C3 |= UART_C3_TXINV_MASK;
    EDMA_ResetChannel(DMA0, UART_TRANSMIT_DMA_CHANNEL);
    EDMA_ResetChannel(DMA0, UART_RECEIVE_DMA_CHANNEL);
    EDMA_CreateHandle(&uart_transmit_dma, DMA0, UART_TRANSMIT_DMA_CHANNEL);
    EDMA_CreateHandle(&uart_receive_dma, DMA0, UART_RECEIVE_DMA_CHANNEL);
    configure_dmamux_channel(UART_TRANSMIT_DMA_CHANNEL, UART_TRANSMIT_DMA_SOURCE, false);
    configure_dmamux_channel(UART_RECEIVE_DMA_CHANNEL, UART_RECEIVE_DMA_SOURCE, false);

    EDMA_PrepareTransfer(&transmit, uart_transmit_window, 1, (void *)&UART1->D, 1, 1,
                         UART_TRANSMIT_SIZE, kEDMA_MemoryToPeripheral);
    EDMA_SetTransferConfig(DMA0, UART_TRANSMIT_DMA_CHANNEL, &transmit, NULL);
    EDMA_SetMajorOffsetConfig(DMA0, UART_TRANSMIT_DMA_CHANNEL, -(int32_t)UART_TRANSMIT_SIZE, 0);
    EDMA_EnableChannelInterrupts(DMA0, UART_TRANSMIT_DMA_CHANNEL, kEDMA_MajorInterruptEnable);
    DMA0->TCD[UART_TRANSMIT_DMA_CHANNEL].CSR |= DMA_CSR_DREQ_MASK;
    configure_dmamux_channel(UART_TRANSMIT_DMA_CHANNEL, UART_TRANSMIT_DMA_SOURCE, true);

    UART_TransferCreateHandleEDMA(UART1, &uart_handle, uart_callback, NULL, &uart_transmit_dma,
                                  &uart_receive_dma);
    UART1->PFIFO &= (uint8_t)~(UART_PFIFO_TXFE_MASK | UART_PFIFO_RXFE_MASK);
    UART_EnableInterrupts(UART1, kUART_RxOverrunInterruptEnable | kUART_NoiseErrorInterruptEnable |
                                     kUART_FramingErrorInterruptEnable |
                                     kUART_ParityErrorInterruptEnable);
    reset_if_failed(configure_uart_receive());
    restore_uart_registers();

    nvic_enable(DMA0_IRQn, 15);
    nvic_enable(DMA1_IRQn, 15);
    NVIC_SetPriority(UART1_RX_TX_IRQn, 0);
    NVIC_DisableIRQ(UART1_RX_TX_IRQn);
    NVIC_ClearPendingIRQ(UART1_RX_TX_IRQn);
    nvic_enable(UART1_ERR_IRQn, 0);
}

/**
 * @brief Initializes periodic and one-shot PIT channels.
 *
 * Starts the 1 ms service tick on channel 0 and enables channel 1 for UART guard delays with their
 * official interrupt priorities.
 */
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

/**
 * @brief Initializes and calibrates the WQR sensor ADC.
 *
 * Runs the official divider-1 continuous calibration sequence with 32-sample averaging, rebuilds
 * both gain registers while preserving their upper halves, and restores divider-2 single-conversion
 * operation before enabling the ADC interrupt.
 */
static void configure_adc(void) {
    adc16_config_t config;
    adc16_config_t calibration;
    uint32_t minus_upper;
    uint32_t plus_upper;
    uint16_t plus_gain;
    uint16_t minus_gain;

    CLOCK_EnableClock(kCLOCK_Adc0);
    ADC0->SC2 = 0;
    ADC16_GetDefaultConfig(&config);
    config.clockSource = kADC16_ClockSourceAlt0;
    config.clockDivider = kADC16_ClockDivider2;
    config.resolution = kADC16_ResolutionSE12Bit;
    config.hardwareAverageMode = kADC16_HardwareAverageCount32;
    ADC16_GetDefaultConfig(&calibration);
    calibration.clockSource = kADC16_ClockSourceAlt0;
    calibration.clockDivider = kADC16_ClockDivider1;
    calibration.resolution = kADC16_ResolutionSE12Bit;
    calibration.hardwareAverageMode = kADC16_HardwareAverageCount32;
    calibration.enableContinuousConversion = true;
    plus_upper = ADC0->PG & UINT32_C(0xffff0000);
    minus_upper = ADC0->MG & UINT32_C(0xffff0000);
    ADC16_Init(ADC0, &calibration);
    reset_if_failed(ADC16_DoAutoCalibration(ADC0));
    plus_gain =
        (uint16_t)(ADC0->CLP0 + ADC0->CLP1 + ADC0->CLP2 + ADC0->CLP3 + ADC0->CLP4 + ADC0->CLPS);
    minus_gain =
        (uint16_t)(ADC0->CLM0 + ADC0->CLM1 + ADC0->CLM2 + ADC0->CLM3 + ADC0->CLM4 + ADC0->CLMS);
    ADC0->PG = plus_upper | UINT16_C(0x8000) | (plus_gain >> 1);
    ADC0->MG = minus_upper | UINT16_C(0x8000) | (minus_gain >> 1);
    ADC16_Init(ADC0, &config);
    ADC0->SC2 = 0;
    nvic_enable(ADC0_IRQn, 9);
}

/**
 * @brief Publishes completion of the active SPI operation.
 *
 * Releases chip select, cancels retry timing, records success or failure, and uses a memory barrier
 * before making completion visible to the protocol service.
 *
 * @param[in] status Final SDK status for the operation.
 */
static void finish_spi(status_t status) {
    GPIO_PinWrite(GPIOC, 4, 1);
    spi_retry_delay = 0;
    spi_transfer_failed = status != kStatus_Success;
    __DMB();
    spi_transfer_complete = true;
}

/**
 * @brief Completes one interrupt-driven 16-bit SPI transfer.
 *
 * Disables the temporary FIFO error interrupts and publishes the SDK completion status through the
 * shared SPI completion path.
 *
 * @param[in,out] base SPI register block supplied by the SDK callback.
 * @param[in] handle DSPI transfer handle supplied by the SDK callback.
 * @param[in] status Completed DSPI transfer status.
 * @param[in] data Callback context supplied during handle creation.
 */
static void spi_word_callback(SPI_Type *base, dspi_master_handle_t *handle, status_t status,
                              void *data) {
    (void)handle;
    (void)data;
    DSPI_DisableInterrupts(base, SPI_ERROR_INTERRUPTS);
    finish_spi(status);
}

/**
 * @brief Selects the active SPI frame width and clock phase.
 *
 * Stops the module, updates only CTAR frame-size and phase fields, clears stale status, and resumes
 * transfer operation.
 *
 * @param[in] bits Number of bits in each SPI frame.
 * @param[in] phase Clock edge used to capture data.
 */
static void set_spi_format(unsigned int bits, dspi_clock_phase_t phase) {
    DSPI_StopTransfer(SPI0);
    SPI0->CTAR[0] = (SPI0->CTAR[0] & ~(SPI_CTAR_FMSZ_MASK | SPI_CTAR_CPHA_MASK)) |
                    SPI_CTAR_FMSZ(bits - 1) |
                    (phase == kDSPI_ClockPhaseSecondEdge ? SPI_CTAR_CPHA_MASK : 0);
    DSPI_ClearStatusFlags(SPI0, kDSPI_AllStatusFlag);
    DSPI_StartTransfer(SPI0);
}

/**
 * @brief Clears the saved SPI command and both FIFOs.
 *
 * Halts the module before zeroing PUSHR and flushing transmit and receive state so the next
 * transfer cannot inherit an alternate-mode command.
 */
static void reset_spi_command(void) {
    DSPI_StopTransfer(SPI0);
    SPI0->PUSHR = 0;
    DSPI_FlushFifo(SPI0, true, true);
}

/**
 * @brief Restores the complete idle SPI hardware configuration.
 *
 * Releases chip select, safely tears down retained module state, initializes the official CTAR and
 * request registers, clears commands and status, starts the idle module, and enables the SPI error
 * and DMA interrupt vectors.
 */
static void initialize_spi_hardware(void) {
    dspi_master_config_t config;

    GPIO_PinWrite(GPIOC, 4, 1);
    if ((SIM->SCGC6 & SIM_SCGC6_SPI0_MASK) != 0) {
        DSPI_StopTransfer(SPI0);
        DSPI_FlushFifo(SPI0, true, true);
    }
    DSPI_MasterGetDefaultConfig(&config);
    config.whichCtar = kDSPI_Ctar0;
    config.ctarConfig.bitsPerFrame = 8;
    config.ctarConfig.cpha = kDSPI_ClockPhaseSecondEdge;
    config.whichPcs = kDSPI_Pcs0;
    config.pcsActiveHighOrLow = kDSPI_PcsActiveLow;
    DSPI_MasterInit(SPI0, &config, BUS_CLOCK);
    SPI0->MCR |= SPI_MCR_DIS_TXF_MASK | SPI_MCR_DIS_RXF_MASK;
    SPI0->TCR = 0;
    SPI0->CTAR[0] = UINT32_C(0x3a514204);
    SPI0->RSER = UINT32_C(0x03030000);
    reset_spi_command();
    DSPI_ClearStatusFlags(SPI0, kDSPI_AllStatusFlag);
    DSPI_StartTransfer(SPI0);
    GPIO_PinWrite(GPIOC, 4, 1);
    NVIC_ClearPendingIRQ(SPI0_IRQn);
    NVIC_ClearPendingIRQ(DMA2_IRQn);
    NVIC_ClearPendingIRQ(DMA3_IRQn);
    NVIC_EnableIRQ(SPI0_IRQn);
    nvic_enable(DMA2_IRQn, 14);
    nvic_enable(DMA3_IRQn, 14);
}

/**
 * @brief Prebuilds the official 33-byte SPI DMA descriptors.
 *
 * Disables each request route while submitting its byte-wide descriptor, installs the source or
 * destination rewind, and enables the route after its descriptor setup is complete.
 */
static void prepare_spi_dma_descriptors(void) {
    edma_transfer_config_t receive;
    edma_transfer_config_t transmit;

    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, false);
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, false);
    EDMA_PrepareTransfer(&transmit, spi_transmit_buffer, 1,
                         (void *)(uintptr_t)DSPI_MasterGetTxRegisterAddress(SPI0), 1, 1,
                         WQR_SPI_TRANSFER_SIZE, kEDMA_MemoryToPeripheral);
    EDMA_PrepareTransfer(&receive, (void *)(uintptr_t)DSPI_GetRxRegisterAddress(SPI0), 1,
                         spi_receive_buffer, 1, 1, WQR_SPI_TRANSFER_SIZE, kEDMA_PeripheralToMemory);
    reset_if_failed(EDMA_SubmitTransfer(&spi_transmit_dma, &transmit));
    DMA0->TCD[SPI_TRANSMIT_DMA_CHANNEL].SLAST = (uint32_t)-(int32_t)WQR_SPI_TRANSFER_SIZE;
    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, true);
    reset_if_failed(EDMA_SubmitTransfer(&spi_receive_dma, &receive));
    DMA0->TCD[SPI_RECEIVE_DMA_CHANNEL].DLAST_SGA = (uint32_t)-(int32_t)WQR_SPI_TRANSFER_SIZE;
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, true);
}

/**
 * @brief Initializes SPI transfer handles, hardware, and DMA descriptors.
 *
 * Creates both eDMA handles and the interrupt-driven word-transfer handle before establishing the
 * official idle module and startup descriptor state.
 */
static void configure_spi(void) {
    EDMA_CreateHandle(&spi_transmit_dma, DMA0, SPI_TRANSMIT_DMA_CHANNEL);
    EDMA_CreateHandle(&spi_receive_dma, DMA0, SPI_RECEIVE_DMA_CHANNEL);
    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, false);
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, false);
    DSPI_MasterTransferCreateHandle(SPI0, &spi_word_handle, spi_word_callback, NULL);

    initialize_spi_hardware();
    prepare_spi_dma_descriptors();
}

/**
 * @brief Starts or restarts the active primary SPI DMA transfer.
 *
 * Quiesces word and DMA paths, clears stale status, restores byte mode and chip select, rebuilds
 * descriptors for the current transfer length, then starts receive before transmit. Submission
 * failure is published through the normal SPI completion path.
 *
 * @return True when both DMA descriptors were submitted and started.
 */
static bool start_spi_dma(void) {
    edma_transfer_config_t receive;
    edma_transfer_config_t transmit;
    status_t receive_status;
    status_t transmit_status;

    NVIC_DisableIRQ(DMA2_IRQn);
    NVIC_DisableIRQ(DMA3_IRQn);
    DSPI_DisableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, false);
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, false);
    DSPI_MasterTransferAbort(SPI0, &spi_word_handle);
    DSPI_DisableInterrupts(SPI0, kDSPI_AllInterruptEnable);
    EDMA_AbortTransfer(&spi_receive_dma);
    EDMA_AbortTransfer(&spi_transmit_dma);
    EDMA_ClearChannelStatusFlags(DMA0, SPI_RECEIVE_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    EDMA_ClearChannelStatusFlags(DMA0, SPI_TRANSMIT_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    NVIC_ClearPendingIRQ(DMA2_IRQn);
    NVIC_ClearPendingIRQ(DMA3_IRQn);
    NVIC_DisableIRQ(SPI0_IRQn);
    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, true);
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, true);
    reset_spi_command();
    set_spi_format(8, kDSPI_ClockPhaseSecondEdge);
    GPIO_PinWrite(GPIOC, 4, 0);

    EDMA_PrepareTransfer(&receive, (void *)(uintptr_t)DSPI_GetRxRegisterAddress(SPI0), 1,
                         spi_receive_buffer, 1, 1, spi_transfer_length, kEDMA_PeripheralToMemory);
    EDMA_PrepareTransfer(&transmit, spi_transmit_buffer, 1,
                         (void *)(uintptr_t)DSPI_MasterGetTxRegisterAddress(SPI0), 1, 1,
                         spi_transfer_length, kEDMA_MemoryToPeripheral);
    spi_transfer_complete = false;
    spi_transfer_failed = false;
    spi_retry_pending = false;
    spi_retry_delay = SPI_RETRY_TICKS;
    receive_status = EDMA_SubmitTransfer(&spi_receive_dma, &receive);
    transmit_status = EDMA_SubmitTransfer(&spi_transmit_dma, &transmit);
    if (receive_status != kStatus_Success || transmit_status != kStatus_Success) {
        EDMA_AbortTransfer(&spi_receive_dma);
        EDMA_AbortTransfer(&spi_transmit_dma);
        finish_spi(kStatus_Fail);
        NVIC_EnableIRQ(DMA2_IRQn);
        NVIC_EnableIRQ(DMA3_IRQn);
        return false;
    }
    DMA0->TCD[SPI_RECEIVE_DMA_CHANNEL].DLAST_SGA = (uint32_t)-(int32_t)spi_transfer_length;
    DMA0->TCD[SPI_TRANSMIT_DMA_CHANNEL].SLAST = (uint32_t)-(int32_t)spi_transfer_length;

    DSPI_EnableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
    EDMA_StartTransfer(&spi_receive_dma);
    EDMA_StartTransfer(&spi_transmit_dma);
    NVIC_EnableIRQ(SPI0_IRQn);
    NVIC_EnableIRQ(DMA2_IRQn);
    NVIC_EnableIRQ(DMA3_IRQn);
    return true;
}

/**
 * @brief Starts or polls one byte-oriented SPI protocol transfer.
 *
 * Copies a new request into stable DMA storage and starts asynchronous transfer. Later calls report
 * pending state or copy the completed receive bytes after the interrupt publication barrier.
 *
 * @param[in] context Unused protocol I/O context.
 * @param[in] transmit Bytes to transmit.
 * @param[out] receive Destination for completed receive bytes.
 * @param[in] length Number of bytes in both buffers.
 * @return Pending while active, succeeded after receive publication, or failed after a transfer
 * error.
 */
static wqr_io_result io_spi_transfer(void *context, const uint8_t *transmit, uint8_t *receive,
                                     size_t length) {
    (void)context;
    if (spi_transfer_active) {
        if (!spi_transfer_complete) {
            return WQR_IO_PENDING;
        }
        __DMB();
        spi_transfer_complete = false;
        spi_transfer_active = false;
        if (spi_transfer_failed) {
            return WQR_IO_FAILED;
        }
        memcpy(receive, spi_receive_buffer, length);
        return WQR_IO_SUCCEEDED;
    }

    memcpy(spi_transmit_buffer, transmit, length);
    spi_transfer_length = length;
    spi_word_active = false;
    spi_transfer_active = true;
    (void)start_spi_dma();
    return WQR_IO_PENDING;
}

/**
 * @brief Starts or polls one interrupt-driven 16-bit SPI transfer.
 *
 * Quiesces the byte-DMA path, selects first-edge 16-bit framing, and begins nonblocking transfer.
 * Later calls publish the received word and terminal result.
 *
 * @param[in] context Unused protocol I/O context.
 * @param[in] transmit Word to transmit.
 * @param[out] receive Destination for the completed receive word.
 * @return Pending while active, succeeded after receive publication, or failed after a transfer
 * error.
 */
static wqr_io_result io_spi_word(void *context, uint16_t transmit, uint16_t *receive) {
    dspi_transfer_t transfer;

    (void)context;
    if (spi_transfer_active) {
        if (!spi_transfer_complete) {
            return WQR_IO_PENDING;
        }
        __DMB();
        spi_transfer_complete = false;
        spi_transfer_active = false;
        *receive = spi_received_word;
        return spi_transfer_failed ? WQR_IO_FAILED : WQR_IO_SUCCEEDED;
    }

    DSPI_DisableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
    NVIC_DisableIRQ(DMA2_IRQn);
    NVIC_DisableIRQ(DMA3_IRQn);
    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, false);
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, false);
    EDMA_AbortTransfer(&spi_receive_dma);
    EDMA_AbortTransfer(&spi_transmit_dma);
    NVIC_ClearPendingIRQ(DMA2_IRQn);
    NVIC_ClearPendingIRQ(DMA3_IRQn);
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

/**
 * @brief Publishes completion of one nonblocking I2C transfer.
 *
 * Restores start/stop detection, exposes successful completion through a memory barrier, and
 * disables transfer interrupts while deferring full peripheral recovery for failures to the main
 * loop.
 *
 * @param[in] base I2C instance supplied by the SDK callback.
 * @param[in] handle I2C transfer handle supplied by the SDK callback.
 * @param[in] status Completed I2C transfer status.
 * @param[in] data Callback context supplied during handle creation.
 */
static void i2c_callback(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *data) {
    (void)base;
    (void)handle;
    (void)data;

    I2C0->FLT |= I2C_FLT_SSIE_MASK;
    if (status == kStatus_Success) {
        __DMB();
        i2c_state = I2C_SUCCEEDED;
        I2C_EnableInterrupts(I2C0, kI2C_GlobalInterruptEnable);
    } else {
        I2C_DisableInterrupts(I2C0, kI2C_GlobalInterruptEnable);
        i2c_recovery_pending = true;
    }
}

/**
 * @brief Consumes the current terminal I2C result.
 *
 * Returns a published success or failure once and restores the phase to idle. Idle and active
 * transfers both report pending so callers can decide whether to start a new request.
 *
 * @return Succeeded or failed for a terminal transfer, otherwise pending.
 */
static wqr_io_result i2c_result(void) {
    if (i2c_state == I2C_SUCCEEDED) {
        __DMB();
        i2c_state = I2C_IDLE;
        return WQR_IO_SUCCEEDED;
    }
    if (i2c_state == I2C_FAILED) {
        i2c_state = I2C_IDLE;
        return WQR_IO_FAILED;
    }

    return WQR_IO_PENDING;
}

/**
 * @brief Starts one guarded nonblocking I2C transfer.
 *
 * Arms the millisecond timeout, marks the transfer pending, and lets the SDK establish its state
 * machine. Immediate submission failure schedules full recovery while preserving asynchronous
 * result delivery.
 *
 * @param[in] transfer SDK transfer descriptor to submit.
 * @return Pending while completion or recovery is outstanding.
 */
static wqr_io_result start_i2c(i2c_master_transfer_t *transfer) {
    i2c_timeout = I2C_TIMEOUT_MILLISECONDS;
    i2c_state = I2C_PENDING;
    I2C_DisableInterrupts(I2C0, kI2C_GlobalInterruptEnable);

    if (I2C_MasterTransferNonBlocking(I2C0, &i2c_handle, transfer) != kStatus_Success) {
        I2C_DisableInterrupts(I2C0, kI2C_GlobalInterruptEnable);
        i2c_recovery_pending = true;
        I2C0->FLT |= I2C_FLT_SSIE_MASK;
    }

    return WQR_IO_PENDING;
}

/**
 * @brief Starts or polls one protocol I2C write.
 *
 * Consumes a terminal result when available and otherwise starts a new transfer only from the idle
 * phase using the wire address converted to the SDK seven-bit form.
 *
 * @param[in] context Unused protocol I/O context.
 * @param[in] address Eight-bit wire address with the direction bit cleared.
 * @param[in] data Bytes to write.
 * @param[in] length Number of bytes to write.
 * @return Current asynchronous I2C result.
 */
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

/**
 * @brief Starts or polls one command-addressed protocol I2C read.
 *
 * Consumes a terminal result when available and otherwise starts a new transfer from the idle
 * phase. A zero-length protocol read performs the official one-byte bus read into discard storage.
 *
 * @param[in] context Unused protocol I/O context.
 * @param[in] address Eight-bit wire address with the direction bit cleared.
 * @param[in] command One-byte I2C subaddress.
 * @param[out] data Destination for received bytes.
 * @param[in] length Number of requested protocol bytes.
 * @return Current asynchronous I2C result.
 */
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
        transfer.data = &i2c_discard;
        transfer.dataSize = 1;
    }

    return start_i2c(&transfer);
}

/**
 * @brief Initializes the WQR I2C master and transfer state.
 *
 * Configures the official bus rate and glitch filter, enables start/stop detection, creates the SDK
 * transfer handle, restores the idle phase, and enables the I2C interrupt.
 */
static void configure_i2c(void) {
    i2c_master_config_t config;

    I2C_MasterGetDefaultConfig(&config);
    config.baudRate_Bps = 857143;
    config.glitchFilterWidth = 10;
    I2C_MasterInit(I2C0, &config, BUS_CLOCK);
    I2C0->FLT |= I2C_FLT_SSIE_MASK;
    I2C_MasterTransferCreateHandle(I2C0, &i2c_handle, i2c_callback, NULL);
    I2C_EnableInterrupts(I2C0, kI2C_GlobalInterruptEnable);
    i2c_state = I2C_IDLE;

    nvic_enable(I2C0_IRQn, 10);
}

/**
 * @brief Performs deferred full I2C peripheral recovery.
 *
 * Reinitializes the controller only when requested and publishes one failed result for the protocol
 * layer to consume.
 */
static void recover_i2c(void) {
    if (!i2c_recovery_pending) {
        return;
    }

    i2c_recovery_pending = false;
    configure_i2c();
    i2c_state = I2C_FAILED;
}

/**
 * @brief Reads the three active-low WQR digital inputs.
 *
 * Packs GPIO A pins 4, 18, and 19 into status bits zero through two.
 *
 * @param[in] context Unused protocol I/O context.
 * @return Three-bit input value.
 */
static uint8_t io_read_inputs(void *context) {
    (void)context;
    return (uint8_t)((GPIO_PinRead(GPIOA, 4) == 0 ? 1 : 0) |
                     (GPIO_PinRead(GPIOA, 18) == 0 ? 2 : 0) |
                     (GPIO_PinRead(GPIOA, 19) == 0 ? 4 : 0));
}

/**
 * @brief Reads the active-low peer-presence input.
 *
 * Converts GPIO C pin 2 to the logical transfer-ready state.
 *
 * @param[in] context Unused protocol I/O context.
 * @return True when the peer-presence input is asserted.
 */
static bool io_transfer_ready(void *context) {
    (void)context;
    return GPIO_PinRead(GPIOC, 2) == 0;
}

/**
 * @brief Reads the peer transfer-control acknowledgement input.
 *
 * Converts GPIO C pin 3 directly to the logical acknowledgement state.
 *
 * @param[in] context Unused protocol I/O context.
 * @return True when transfer control is acknowledged.
 */
static bool io_transfer_control_ready(void *context) {
    (void)context;
    return GPIO_PinRead(GPIOC, 3) != 0;
}

/**
 * @brief Drives the local transfer-control output.
 *
 * Writes the requested logical level to GPIO C pin 3.
 *
 * @param[in] context Unused protocol I/O context.
 * @param[in] asserted Requested output state.
 */
static void io_set_transfer_control(void *context, bool asserted) {
    (void)context;
    GPIO_PinWrite(GPIOC, 3, asserted ? 1 : 0);
}

/**
 * @brief Resets all platform SPI transfer state.
 *
 * Quiesces interrupts, DMA, descriptors, FIFOs, buffers, retries, and completion flags before
 * restoring the official idle SPI hardware and startup descriptors.
 *
 * @param[in] context Unused protocol I/O context.
 */
static void io_reset_transfer(void *context) {
    (void)context;
    NVIC_DisableIRQ(DMA2_IRQn);
    NVIC_DisableIRQ(DMA3_IRQn);
    NVIC_DisableIRQ(SPI0_IRQn);
    DSPI_MasterTransferAbort(SPI0, &spi_word_handle);
    DSPI_DisableInterrupts(SPI0, SPI_ERROR_INTERRUPTS);
    DSPI_DisableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
    configure_dmamux_channel(SPI_TRANSMIT_DMA_CHANNEL, SPI_TRANSMIT_DMA_SOURCE, false);
    configure_dmamux_channel(SPI_RECEIVE_DMA_CHANNEL, SPI_RECEIVE_DMA_SOURCE, false);
    EDMA_AbortTransfer(&spi_receive_dma);
    EDMA_AbortTransfer(&spi_transmit_dma);
    EDMA_ClearChannelStatusFlags(DMA0, SPI_RECEIVE_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    EDMA_ClearChannelStatusFlags(DMA0, SPI_TRANSMIT_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    NVIC_ClearPendingIRQ(DMA2_IRQn);
    NVIC_ClearPendingIRQ(DMA3_IRQn);
    NVIC_ClearPendingIRQ(SPI0_IRQn);
    memset(spi_transmit_buffer, 0, sizeof(spi_transmit_buffer));
    memset(spi_receive_buffer, 0, sizeof(spi_receive_buffer));
    spi_received_word = 0;
    spi_transfer_complete = false;
    spi_transfer_failed = false;
    spi_transfer_active = false;
    spi_word_active = false;
    spi_retry_pending = false;
    spi_retry_delay = 0;
    initialize_spi_hardware();
    prepare_spi_dma_descriptors();
}

/**
 * @brief Defers a protocol-requested system reset.
 *
 * Marks the reset for execution from the main loop after the response transport has completed.
 *
 * @param[in] context Unused protocol I/O context.
 */
static void io_request_reset(void *context) {
    (void)context;
    reset_pending = true;
}

/**
 * @brief Programs the official WQR watchdog state.
 *
 * Unlocks the module, applies the official control-bit update, clears its interrupt flag, selects
 * no prescaling, and writes the low timeout half while retaining the official high half.
 */
static void configure_watchdog(void) {
    WDOG->UNLOCK = WATCHDOG_UNLOCK_FIRST;
    WDOG->UNLOCK = WATCHDOG_UNLOCK_SECOND;
    WDOG->STCTRLH |= WATCHDOG_CONTROL_SET;
    WDOG->STCTRLH &= (uint16_t)~WATCHDOG_CONTROL_CLEAR;
    WDOG->STCTRLL &= (uint16_t)~WDOG_STCTRLL_INTFLG_MASK;
    WDOG->PRESC = 0;
    WDOG->TOVALL = WATCHDOG_TIMEOUT_LOW;
}

/**
 * @brief Initializes the framed UART transmit window.
 *
 * Clears the complete 72-byte buffer and fills the four-byte leading and trailing guards with the
 * official `0xF0` value.
 */
static void prepare_transmit_window(void) {
    memset(uart_transmit_window, 0, sizeof(uart_transmit_window));
    memset(uart_transmit_window, 0xf0, UART_FRAME_OFFSET);
    memset(uart_transmit_window + UART_TRANSMIT_SIZE - UART_FRAME_OFFSET, 0xf0, UART_FRAME_OFFSET);
}

/**
 * @brief Consumes UART transmit DMA completion or error state.
 *
 * Clears stale inactive flags, starts guarded recovery after an active DMA error, or finalizes a
 * successful response and schedules the post-transmit guard before receive rearming.
 *
 * @return True only when an active response completed successfully.
 */
static bool finish_uart_transmit(void) {
    uint32_t flags = EDMA_GetChannelStatusFlags(DMA0, UART_TRANSMIT_DMA_CHANNEL);

    if (!uart_transmit_active) {
        if (flags != 0) {
            UART_TransferAbortSendEDMA(UART1, &uart_handle);
            EDMA_ClearChannelStatusFlags(DMA0, UART_TRANSMIT_DMA_CHANNEL,
                                         kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
            restore_uart_registers();
        }
        return false;
    }
    if ((flags & kEDMA_ErrorFlag) != 0) {
        UART_TransferAbortSendEDMA(UART1, &uart_handle);
        EDMA_ClearChannelStatusFlags(DMA0, UART_TRANSMIT_DMA_CHANNEL,
                                     kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
        uart_transmit_active = false;
        uart_response_due = true;
        restore_uart_registers();
        start_uart_recovery();
        return false;
    }
    if ((flags & (kEDMA_DoneFlag | kEDMA_InterruptFlag)) == 0) {
        return false;
    }
    UART_EnableTxDMA(UART1, false);
    EDMA_ClearChannelStatusFlags(DMA0, UART_TRANSMIT_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    UART_DisableInterrupts(UART1, kUART_TransmissionCompleteInterruptEnable);
    UART_TransferAbortSendEDMA(UART1, &uart_handle);
    restore_uart_registers();
    NVIC_ClearPendingIRQ(UART1_RX_TX_IRQn);
    NVIC_ClearPendingIRQ(DMA0_IRQn);
    uart_transmit_active = false;
    uart_response_sent_pending = true;
    if (!uart_recovery_active) {
        start_uart_guard(UART_RESPONSE_GUARD_TICKS);
    }
    return true;
}

/**
 * @brief Handles UART transmit DMA completion.
 *
 * Routes channel 0 interrupt state through the shared transmit completion and recovery path.
 */
void DMA0_IRQHandler(void) { (void)finish_uart_transmit(); }

/**
 * @brief Handles UART receive DMA completion.
 *
 * Accepts completion status, clears done and interrupt flags before aborting the one-shot SDK
 * receive, and publishes the completed window through the UART callback. Stale or error-only state
 * is discarded.
 */
void DMA1_IRQHandler(void) {
    if ((EDMA_GetChannelStatusFlags(DMA0, UART_RECEIVE_DMA_CHANNEL) &
         (kEDMA_DoneFlag | kEDMA_InterruptFlag)) != 0) {
        EDMA_ClearChannelStatusFlags(DMA0, UART_RECEIVE_DMA_CHANNEL,
                                     kEDMA_DoneFlag | kEDMA_InterruptFlag);
        UART_TransferAbortReceiveEDMA(UART1, &uart_handle);
        uart_callback(UART1, &uart_handle, kStatus_UART_RxIdle, NULL);
    } else {
        EDMA_ClearChannelStatusFlags(DMA0, UART_RECEIVE_DMA_CHANNEL,
                                     kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    }
}

/**
 * @brief Handles primary SPI transmit DMA completion.
 *
 * Clears channel 2 done and interrupt status while receive-channel completion remains authoritative
 * for the full-duplex transfer result.
 */
void DMA2_IRQHandler(void) {
    EDMA_ClearChannelStatusFlags(DMA0, SPI_TRANSMIT_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_InterruptFlag);
}

/**
 * @brief Handles primary SPI receive DMA completion.
 *
 * Rejects stale and word-mode interrupts, publishes successful byte-transfer completion, releases
 * chip select, and disables further DSPI DMA requests.
 */
void DMA3_IRQHandler(void) {
    if ((EDMA_GetChannelStatusFlags(DMA0, SPI_RECEIVE_DMA_CHANNEL) & kEDMA_InterruptFlag) == 0) {
        return;
    }
    EDMA_ClearChannelStatusFlags(DMA0, SPI_RECEIVE_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_InterruptFlag);
    if (!spi_transfer_active || spi_word_active) {
        return;
    }
    finish_spi(kStatus_Success);
    DSPI_DisableDMA(SPI0, kDSPI_RxDmaEnable | kDSPI_TxDmaEnable);
}

/**
 * @brief Handles interrupt-driven SPI word progress and FIFO errors.
 *
 * Delegates ordinary transfer events to the SDK state machine. FIFO underflow or overflow aborts
 * the transfer, clears the reported flags, and publishes failure.
 */
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

/**
 * @brief Handles WQR I2C status and transfer interrupts.
 *
 * Clears start and stop detection, gives arbitration loss precedence, consumes master-generated
 * detection events, rejects idle interrupts, and advances the SDK state machine only for an active
 * transfer interrupt.
 */
void I2C0_IRQHandler(void) {
    uint8_t detection = I2C0->FLT & (I2C_FLT_STARTF_MASK | I2C_FLT_STOPF_MASK);
    uint8_t status = I2C0->S;

    I2C0->FLT |= detection;
    if ((status & I2C_S_ARBL_MASK) != 0) {
        I2C_MasterClearStatusFlags(I2C0, kI2C_ArbitrationLostFlag);
        return;
    }
    if (detection != 0 && (I2C0->C1 & I2C_C1_MST_MASK) != 0) {
        I2C_MasterClearStatusFlags(I2C0, kI2C_IntPendingFlag);
        return;
    }
    if ((status & I2C_S_IICIF_MASK) == 0) {
        return;
    }
    if (i2c_state != I2C_PENDING) {
        I2C_MasterClearStatusFlags(I2C0, kI2C_IntPendingFlag);
        return;
    }
    I2C_MasterTransferHandleIRQ(I2C0, &i2c_handle);
}

/**
 * @brief Handles UART receive error interrupts.
 *
 * Drains all currently latched parity, framing, noise, and overrun conditions.
 */
void UART1_ERR_DriverIRQHandler(void) { drain_uart_errors(); }

/**
 * @brief Advances the active I2C timeout by one millisecond.
 *
 * Masks the I2C vector while updating shared state and schedules full recovery when the pending
 * transfer reaches zero remaining ticks.
 */
static void update_i2c_timeout(void) {
    NVIC_DisableIRQ(I2C0_IRQn);
    if (i2c_state != I2C_PENDING || i2c_timeout == 0) {
        NVIC_EnableIRQ(I2C0_IRQn);
        return;
    }
    --i2c_timeout;
    if (i2c_timeout == 0) {
        I2C_DisableInterrupts(I2C0, kI2C_GlobalInterruptEnable);
        i2c_recovery_pending = true;
    }
    NVIC_EnableIRQ(I2C0_IRQn);
}

/**
 * @brief Advances the primary SPI retry countdown by one millisecond.
 *
 * Ignores inactive, word-mode, completed, or unarmed transfers and publishes a retry request when
 * the remaining delay reaches zero.
 */
static void update_spi_retry(void) {
    if (!spi_transfer_active || spi_word_active || spi_transfer_complete || spi_retry_delay == 0) {
        return;
    }
    if (--spi_retry_delay == 0) {
        spi_retry_pending = true;
    }
}

/**
 * @brief Restarts a primary SPI transfer whose retry delay expired.
 *
 * Discards stale retry requests and otherwise rebuilds and starts the current byte-DMA operation.
 */
static void restart_spi_if_due(void) {
    if (!spi_retry_pending || !spi_transfer_active || spi_word_active || spi_transfer_complete) {
        spi_retry_pending = false;
        return;
    }
    (void)start_spi_dma();
}

/**
 * @brief Handles the 1 ms firmware service tick.
 *
 * Advances protocol uptime, SPI retry and I2C timeout state, starts a sensor conversion every 100
 * milliseconds, and clears the PIT channel flag.
 */
void PIT0_IRQHandler(void) {
    wqr_protocol_tick(&protocol);
    update_spi_retry();
    update_i2c_timeout();
    if (protocol.milliseconds % 100 == 0) {
        const adc16_channel_config_t channel = {
            .channelNumber = 23,
            .enableInterruptOnConversionCompleted = true,
        };
        ADC16_SetChannelConfig(ADC0, 0, &channel);
    }
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
}

/**
 * @brief Handles expiration of the UART response or recovery guard.
 *
 * Stops the one-shot timer, reports completed response transmission, drains receive errors, and
 * rearms the receive descriptor when no response is already due. Busy or failed rearm attempts are
 * aborted or rescheduled through guarded recovery.
 */
void PIT1_IRQHandler(void) {
    bool response_sent = uart_response_sent_pending;
    status_t status;

    PIT_ClearStatusFlags(PIT, kPIT_Chnl_1, kPIT_TimerFlag);
    PIT_StopTimer(PIT, kPIT_Chnl_1);
    uart_recovery_active = false;
    if (!response_sent && uart_response_due) {
        return;
    }
    if (response_sent) {
        uart_response_sent_pending = false;
        wqr_protocol_response_sent(&protocol);
    }
    drain_uart_errors();
    status = rearm_uart_receive();
    if (status != kStatus_Success) {
        start_uart_recovery();
    }
}

/**
 * @brief Handles completion of one sensor ADC conversion.
 *
 * Captures the conversion result and publishes it to the main loop.
 */
void ADC0_IRQHandler(void) {
    adc_sample = (uint16_t)ADC16_GetChannelConversionValue(ADC0, 0);
    adc_sample_ready = true;
}

/**
 * @brief Handles the watchdog interrupt vector.
 *
 * Intentionally performs no recovery so watchdog expiry proceeds through the configured hardware
 * reset behavior.
 */
void WDOG_EWM_IRQHandler(void) {}

/**
 * @brief Extracts and submits one frame from the UART receive window.
 *
 * Searches the four-byte leading guard for the frame start, validates the matching end marker,
 * starts recovery for an invalid window, submits the selected bytes to the protocol parser, and
 * marks response work due.
 */
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

/**
 * @brief Starts transmission of the next queued protocol response.
 *
 * Starts only when a response is due and neither transmission nor recovery is active, builds the
 * framed response, disables receive requests, clears stale transmit status, submits the 72-byte DMA
 * window, restores descriptor rewind and request routing, and enters guarded recovery on submission
 * failure.
 */
static void start_uart_response(void) {
    uart_transfer_t transfer;

    if (!uart_response_due || uart_transmit_active || uart_recovery_active) {
        return;
    }
    if (!wqr_protocol_response(&protocol, uart_transmit_window + UART_FRAME_OFFSET)) {
        if (!wqr_protocol_response_expected(&protocol)) {
            uart_response_due = false;
            start_uart_guard(UART_RESPONSE_GUARD_TICKS);
        }
        return;
    }
    transfer.data = uart_transmit_window;
    transfer.dataSize = UART_TRANSMIT_SIZE;
    NVIC_DisableIRQ(DMA0_IRQn);
    EDMA_ClearChannelStatusFlags(DMA0, UART_TRANSMIT_DMA_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag | kEDMA_InterruptFlag);
    NVIC_ClearPendingIRQ(DMA0_IRQn);

    EDMA_DisableChannelRequest(DMA0, UART_RECEIVE_DMA_CHANNEL);
    configure_dmamux_channel(UART_TRANSMIT_DMA_CHANNEL, UART_TRANSMIT_DMA_SOURCE, false);
    if (UART_SendEDMA(UART1, &uart_handle, &transfer) != kStatus_Success) {
        UART_TransferAbortSendEDMA(UART1, &uart_handle);
        configure_dmamux_channel(UART_TRANSMIT_DMA_CHANNEL, UART_TRANSMIT_DMA_SOURCE, true);
        restore_uart_registers();
        NVIC_EnableIRQ(DMA0_IRQn);
        start_uart_recovery();
        return;
    }
    DMA0->TCD[UART_TRANSMIT_DMA_CHANNEL].SLAST = (uint32_t)-(int32_t)UART_TRANSMIT_SIZE;
    configure_dmamux_channel(UART_TRANSMIT_DMA_CHANNEL, UART_TRANSMIT_DMA_SOURCE, true);
    restore_uart_registers();
    uart_transmit_active = true;
    uart_response_due = false;
    NVIC_EnableIRQ(DMA0_IRQn);
}

/**
 * @brief Initializes and runs the WQR firmware.
 *
 * Establishes clocks, pins, DMA, ADC, SPI, I2C, protocol, UART, timers, and watchdog state before
 * enabling interrupts. The main loop then consumes interrupt publications, services protocol and
 * peripheral state machines, transmits responses, performs deferred reset, and refreshes the
 * watchdog.
 */
void firmware_main(void) {
    const wqr_io io = {
        .spi_transfer = io_spi_transfer,
        .spi_word = io_spi_word,
        .i2c_write = io_i2c_write,
        .i2c_read = io_i2c_read,
        .read_inputs = io_read_inputs,
        .transfer_ready = io_transfer_ready,
        .transfer_control_ready = io_transfer_control_ready,
        .set_transfer_control = io_set_transfer_control,
        .reset_transfer = io_reset_transfer,
        .request_reset = io_request_reset,
    };
    edma_config_t dma;

    __disable_irq();
    configure_clock();
    configure_pins();
    prepare_transmit_window();

    EDMA_GetDefaultConfig(&dma);
    dma.enableHaltOnError = false;
    EDMA_Init(DMA0, &dma);
    DMA0->CR = 0;
    DMAMUX_Init(DMAMUX);

    configure_adc();
    configure_spi();
    configure_i2c();
    wqr_protocol_init(&protocol, &io);
    configure_uart();
    configure_pit();
    configure_watchdog();
    __enable_irq();

    for (;;) {
        bool uart_frame_processed = false;

        NVIC_DisableIRQ(DMA0_IRQn);
        (void)finish_uart_transmit();
        NVIC_EnableIRQ(DMA0_IRQn);
        recover_i2c();
        if (adc_sample_ready) {
            adc_sample_ready = false;
            wqr_protocol_set_sensor_sample(&protocol, adc_sample);
        }
        if (uart_receive_ready) {
            __DMB();
            uart_receive_ready = false;
            process_uart_frame();
            uart_frame_processed = true;
        }
        wqr_protocol_poll(&protocol);
        restart_spi_if_due();

        if (!uart_frame_processed) {
            start_uart_response();
        }
        if (reset_pending) {
            NVIC_SystemReset();
        }
        WDOG_Refresh(WDOG);
    }
}
