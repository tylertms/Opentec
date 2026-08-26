#include <stdint.h>

#include "MK22F12810.h"
#include "protocol.h"

enum {
    UART_WINDOW_SIZE = 68,
    UART_TRANSMIT_SIZE = 72,
    UART_FRAME_OFFSET = 4,
    UART_SOURCE_CLOCK = 96000000,
    UART_BAUD_RATE = 5000000,
    PIT_TICKS_PER_MILLISECOND = 24000,
    SPI_TIMEOUT = 100000,
    I2C_TIMEOUT = 100000,
    FPU_ACCESS_MASK = (3u << 20) | (3u << 22),
    WDOG_REQUIRED_MASK = 0x100
};

uint32_t SystemCoreClock = DEFAULT_SYSTEM_CLOCK;

static wqr_protocol protocol;
static uint8_t uart_receive_window[UART_WINDOW_SIZE] __attribute__((aligned(4)));
static uint8_t uart_transmit_window[UART_TRANSMIT_SIZE] __attribute__((aligned(4)));
static volatile bool uart_receive_ready;
static bool uart_transmit_active;
static bool uart_response_due;
static volatile bool adc_sample_ready;
static volatile uint16_t adc_sample;

void SystemInit(void) {
    SCB->CPACR |= FPU_ACCESS_MASK;
    WDOG->UNLOCK = WDOG_UNLOCK_WDOGUNLOCK(0xc520);
    WDOG->UNLOCK = WDOG_UNLOCK_WDOGUNLOCK(0xd928);
    WDOG->STCTRLH = WDOG_REQUIRED_MASK | WDOG_STCTRLH_WAITEN_MASK | WDOG_STCTRLH_STOPEN_MASK |
                    WDOG_STCTRLH_ALLOWUPDATE_MASK | WDOG_STCTRLH_CLKSRC_MASK;
}

static void nvic_enable(IRQn_Type interrupt, uint32_t priority) {
    NVIC_SetPriority(interrupt, priority);
    NVIC_EnableIRQ(interrupt);
}

static void configure_clock(void) {
    SIM->CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0) | SIM_CLKDIV1_OUTDIV2(3) | SIM_CLKDIV1_OUTDIV4(3);
    MCG->C8 = 0;
    MCG->C2 = (uint8_t)((MCG->C2 & ~MCG_C2_RANGE_MASK) | MCG_C2_RANGE(1));
    OSC->CR = OSC_CR_ERCLKEN_MASK;
    MCG->C7 = MCG_C7_OSCSEL(2);
    MCG->C1 = MCG_C1_FRDIV(6) | MCG_C1_IRCLKEN_MASK;
    while ((MCG->S & MCG_S_IREFST_MASK) != 0) {
    }
    MCG->C4 =
        (uint8_t)((MCG->C4 & ~(MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS_MASK)) | MCG_C4_DRST_DRS(3));
    MCG->C6 = 0;
    while ((MCG->S & MCG_S_CLKST_MASK) != 0) {
    }
    SystemCoreClock = UART_SOURCE_CLOCK;
}

static void configure_pin(PORT_Type *port, unsigned int pin, uint32_t value) {
    port->PCR[pin] = value;
}

static void configure_pins(void) {
    uint32_t input_config =
        PORT_PCR_MUX(1) | PORT_PCR_PFE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK |
                  SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK;
    configure_pin(PORTE, 16, PORT_PCR_MUX(3));
    configure_pin(PORTE, 17, PORT_PCR_MUX(3));
    configure_pin(PORTC, 5, PORT_PCR_MUX(2));
    configure_pin(PORTC, 6, PORT_PCR_MUX(2));
    configure_pin(PORTC, 7, PORT_PCR_MUX(2));
    configure_pin(PORTC, 4, PORT_PCR_MUX(1));
    configure_pin(PORTC, 1, PORT_PCR_MUX(1));
    configure_pin(PORTC, 2, PORT_PCR_MUX(1));
    configure_pin(PORTC, 3, PORT_PCR_MUX(1));
    configure_pin(PORTB, 0, PORT_PCR_MUX(2) | PORT_PCR_ODE_MASK);
    configure_pin(PORTB, 1, PORT_PCR_MUX(2) | PORT_PCR_ODE_MASK);
    configure_pin(PORTA, 4, input_config);
    configure_pin(PORTA, 18, input_config);
    configure_pin(PORTA, 19, input_config);
    GPIOC->PDDR |= UINT32_C(0x1a);
    GPIOC->PSOR = UINT32_C(0x10);
    GPIOC->PCOR = UINT32_C(0x08);
}

static void configure_dma_descriptor(unsigned int channel, volatile const void *source,
                                     int16_t source_offset, volatile void *destination,
                                     int16_t destination_offset, uint16_t count) {
    DMA0->TCD[channel].SADDR = (uint32_t)(uintptr_t)source;
    DMA0->TCD[channel].SOFF = (uint16_t)source_offset;
    DMA0->TCD[channel].ATTR = 0;
    DMA0->TCD[channel].NBYTES_MLNO = 1;
    DMA0->TCD[channel].SLAST = -(int32_t)(source_offset * count);
    DMA0->TCD[channel].DADDR = (uint32_t)(uintptr_t)destination;
    DMA0->TCD[channel].DOFF = (uint16_t)destination_offset;
    DMA0->TCD[channel].CITER_ELINKNO = count;
    DMA0->TCD[channel].DLAST_SGA = -(int32_t)(destination_offset * count);
    DMA0->TCD[channel].CSR = DMA_CSR_INTMAJOR_MASK | DMA_CSR_DREQ_MASK;
    DMA0->TCD[channel].BITER_ELINKNO = count;
}

static void configure_uart_receive(void) {
    DMAMUX->CHCFG[1] = 0;
    configure_dma_descriptor(1, &UART1->D, 0, uart_receive_window, 1, UART_WINDOW_SIZE);
    DMAMUX->CHCFG[1] = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(4);
    DMA0->CDNE = DMA_CDNE_CDNE(1);
    DMA0->SERQ = DMA_SERQ_SERQ(1);
}

static void configure_uart(void) {
    uint32_t divisor = UART_SOURCE_CLOCK / (UART_BAUD_RATE * 16);
    uint32_t fraction = (UART_SOURCE_CLOCK * 2 / UART_BAUD_RATE) & 0x1f;

    SIM->SCGC4 |= SIM_SCGC4_UART1_MASK;
    SIM->SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
    SIM->SCGC7 |= SIM_SCGC7_DMA_MASK;
    UART1->BDH = (uint8_t)((UART1->BDH & ~UART_BDH_SBR_MASK) | UART_BDH_SBR(divisor >> 8));
    UART1->BDL = (uint8_t)divisor;
    UART1->C4 = (uint8_t)((UART1->C4 & ~UART_C4_BRFA_MASK) | UART_C4_BRFA(fraction));
    UART1->C1 = 0;
    UART1->S2 = UART_S2_RXINV_MASK;
    UART1->C3 = UART_C3_TXINV_MASK | UART_C3_PEIE_MASK | UART_C3_FEIE_MASK | UART_C3_NEIE_MASK |
                UART_C3_ORIE_MASK;
    UART1->C2 = UART_C2_TE_MASK | UART_C2_RE_MASK | UART_C2_RIE_MASK | UART_C2_TIE_MASK;
    UART1->C5 = UART_C5_TDMAS_MASK | UART_C5_RDMAS_MASK;
    DMAMUX->CHCFG[0] = 0;
    configure_dma_descriptor(0, uart_transmit_window, 1, &UART1->D, 0, UART_TRANSMIT_SIZE);
    DMAMUX->CHCFG[0] = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(5);
    configure_uart_receive();
    nvic_enable(DMA0_IRQn, 15);
    nvic_enable(DMA1_IRQn, 15);
    nvic_enable(UART1_ERR_IRQn, 15);
}

static void configure_pit(void) {
    SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
    PIT->MCR = 0;
    PIT->CHANNEL[0].LDVAL = PIT_TICKS_PER_MILLISECOND;
    PIT->CHANNEL[0].TCTRL = PIT_TCTRL_TEN_MASK | PIT_TCTRL_TIE_MASK;
    PIT->CHANNEL[1].TCTRL = PIT_TCTRL_TIE_MASK;
    nvic_enable(PIT0_IRQn, 11);
    nvic_enable(PIT1_IRQn, 13);
}

static void configure_adc(void) {
    uint32_t positive_gain;
    uint32_t negative_gain;

    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    ADC0->CFG1 = (ADC0->CFG1 & ~(ADC_CFG1_MODE_MASK | ADC_CFG1_ADIV_MASK)) | ADC_CFG1_MODE(1);
    ADC0->SC3 = ADC_SC3_CAL_MASK | ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3);
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0) {
    }
    ADC0->SC3 &= ~ADC_SC3_CALF_MASK;
    positive_gain = ADC0->CLPS + ADC0->CLP4 + ADC0->CLP3 + ADC0->CLP2 + ADC0->CLP1 + ADC0->CLP0;
    negative_gain = ADC0->CLMS + ADC0->CLM4 + ADC0->CLM3 + ADC0->CLM2 + ADC0->CLM1 + ADC0->CLM0;
    ADC0->PG = UINT32_C(0x8000) | (positive_gain >> 1);
    ADC0->MG = UINT32_C(0x8000) | (negative_gain >> 1);
    ADC0->CFG1 = ADC_CFG1_MODE(1) | ADC_CFG1_ADIV(1);
    ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3);
    ADC0->SC2 = 0;
    nvic_enable(ADC0_IRQn, 9);
}

static void configure_spi(void) {
    SIM->SCGC6 |= SIM_SCGC6_SPI0_MASK;
    SPI0->MCR = SPI_MCR_MSTR_MASK | SPI_MCR_PCSIS(1) | SPI_MCR_DIS_RXF_MASK | SPI_MCR_DIS_TXF_MASK |
                SPI_MCR_HALT_MASK;
    SPI0->CTAR[0] = UINT32_C(0x3a514204);
    SPI0->RSER = 0;
    SPI0->MCR &= ~SPI_MCR_HALT_MASK;
}

static bool spi_byte(uint8_t transmit, uint8_t *receive) {
    uint32_t timeout = SPI_TIMEOUT;

    while ((SPI0->SR & SPI_SR_TFFF_MASK) == 0 && --timeout != 0) {
    }
    if (timeout == 0) {
        return false;
    }
    SPI0->PUSHR = transmit;
    timeout = SPI_TIMEOUT;
    while ((SPI0->SR & SPI_SR_RFDF_MASK) == 0 && --timeout != 0) {
    }
    if (timeout == 0) {
        return false;
    }
    *receive = (uint8_t)SPI0->POPR;
    SPI0->SR = SPI_SR_TFFF_MASK | SPI_SR_RFDF_MASK;
    return true;
}

static bool io_spi_transfer(void *context, const uint8_t *transmit, uint8_t *receive,
                            size_t length) {
    size_t index;

    (void)context;
    GPIOC->PCOR = UINT32_C(0x10);
    for (index = 0; index < length; ++index) {
        if (!spi_byte(transmit[index], receive + index)) {
            GPIOC->PSOR = UINT32_C(0x10);
            return false;
        }
    }
    GPIOC->PSOR = UINT32_C(0x10);
    return true;
}

static bool io_spi_word(void *context, uint16_t transmit, uint16_t *receive) {
    uint8_t input[2] = {(uint8_t)transmit, (uint8_t)(transmit >> 8)};
    uint8_t output[2];
    bool success = io_spi_transfer(context, input, output, sizeof(input));

    *receive = (uint16_t)output[0] | (uint16_t)((uint16_t)output[1] << 8);
    return success;
}

static bool i2c_wait(void) {
    uint32_t timeout = I2C_TIMEOUT;

    while ((I2C0->S & I2C_S_IICIF_MASK) == 0 && --timeout != 0) {
    }
    I2C0->S |= I2C_S_IICIF_MASK;
    return timeout != 0 && (I2C0->S & I2C_S_RXAK_MASK) == 0;
}

static bool i2c_send(uint8_t value) {
    I2C0->D = value;
    return i2c_wait();
}

static void i2c_stop(void) { I2C0->C1 = I2C_C1_IICEN_MASK; }

static bool io_i2c_write(void *context, uint8_t address, const uint8_t *data, size_t length) {
    size_t index;

    (void)context;
    I2C0->C1 = I2C_C1_IICEN_MASK | I2C_C1_MST_MASK | I2C_C1_TX_MASK;
    if (!i2c_send(address)) {
        i2c_stop();
        return false;
    }
    for (index = 0; index < length; ++index) {
        if (!i2c_send(data[index])) {
            i2c_stop();
            return false;
        }
    }
    i2c_stop();
    return true;
}

static bool io_i2c_read(void *context, uint8_t address, uint8_t command, uint8_t *data,
                        size_t length) {
    size_t index;

    (void)context;
    if (length == 0) {
        return true;
    }
    I2C0->C1 = I2C_C1_IICEN_MASK | I2C_C1_MST_MASK | I2C_C1_TX_MASK;
    if (!i2c_send(address) || !i2c_send(command)) {
        i2c_stop();
        return false;
    }
    I2C0->C1 = I2C_C1_IICEN_MASK | I2C_C1_MST_MASK | I2C_C1_TX_MASK | I2C_C1_RSTA_MASK;
    if (!i2c_send((uint8_t)(address | 1))) {
        i2c_stop();
        return false;
    }
    I2C0->C1 = I2C_C1_IICEN_MASK | (length == 1 ? I2C_C1_TXAK_MASK : 0);
    (void)I2C0->D;
    for (index = 0; index < length; ++index) {
        if (!i2c_wait()) {
            i2c_stop();
            return false;
        }
        if (index + 2 == length) {
            I2C0->C1 |= I2C_C1_TXAK_MASK;
        }
        if (index + 1 == length) {
            i2c_stop();
        }
        data[index] = I2C0->D;
    }
    return true;
}

static void configure_i2c(void) {
    SIM->SCGC4 |= SIM_SCGC4_I2C0_MASK;
    I2C0->F = I2C_F_ICR(4);
    I2C0->C1 = I2C_C1_IICEN_MASK;
    I2C0->FLT = I2C_FLT_FLT(10) | I2C_FLT_SSIE_MASK;
}

static uint8_t io_read_inputs(void *context) {
    uint32_t inputs = GPIOA->PDIR;

    (void)context;
    return (uint8_t)(((inputs & 0x10) == 0 ? 1 : 0) | ((inputs & 0x40000) == 0 ? 2 : 0) |
                     ((inputs & 0x80000) == 0 ? 4 : 0));
}

static bool io_transfer_ready(void *context) {
    (void)context;
    return (GPIOC->PDIR & UINT32_C(0x04)) == 0;
}

static void io_set_transfer_control(void *context, bool asserted) {
    (void)context;
    if (asserted) {
        GPIOC->PSOR = UINT32_C(0x08);
    } else {
        GPIOC->PCOR = UINT32_C(0x08);
    }
}

static void io_request_reset(void *context) {
    (void)context;
    NVIC_SystemReset();
}

static void configure_watchdog(void) {
    WDOG->UNLOCK = WDOG_UNLOCK_WDOGUNLOCK(0xc520);
    WDOG->UNLOCK = WDOG_UNLOCK_WDOGUNLOCK(0xd928);
    WDOG->STCTRLH = WDOG_STCTRLH_DISTESTWDOG_MASK | WDOG_STCTRLH_WAITEN_MASK |
                    WDOG_STCTRLH_STOPEN_MASK | WDOG_STCTRLH_ALLOWUPDATE_MASK |
                    WDOG_STCTRLH_IRQRSTEN_MASK | WDOG_STCTRLH_WDOGEN_MASK;
    WDOG->TOVALH = 0;
    WDOG->TOVALL = 500;
}

static void refresh_watchdog(void) {
    uint32_t interrupt_mask = __get_PRIMASK();

    __disable_irq();
    WDOG->REFRESH = WDOG_REFRESH_WDOGREFRESH(0xa602);
    WDOG->REFRESH = WDOG_REFRESH_WDOGREFRESH(0xb480);
    __set_PRIMASK(interrupt_mask);
}

static void prepare_transmit_window(void) {
    unsigned int index;

    for (index = 0; index < UART_TRANSMIT_SIZE; ++index) {
        uart_transmit_window[index] = 0;
    }
    for (index = 0; index < UART_FRAME_OFFSET; ++index) {
        uart_transmit_window[index] = 0xf0;
        uart_transmit_window[UART_TRANSMIT_SIZE - index - 1] = 0xf0;
    }
}

void DMA0_IRQHandler(void) {
    DMA0->CINT = DMA_CINT_CINT(0);
    DMA0->CDNE = DMA_CDNE_CDNE(0);
    uart_transmit_active = false;
    wqr_protocol_response_sent(&protocol);
    PIT->CHANNEL[1].LDVAL = 471;
    PIT->CHANNEL[1].TCTRL = PIT_TCTRL_TEN_MASK | PIT_TCTRL_TIE_MASK;
}

void DMA1_IRQHandler(void) {
    DMA0->CINT = DMA_CINT_CINT(1);
    DMA0->CDNE = DMA_CDNE_CDNE(1);
    uart_receive_ready = true;
}

void PIT0_IRQHandler(void) {
    static uint8_t adc_period;

    wqr_protocol_tick(&protocol);
    if (++adc_period == 100) {
        adc_period = 0;
        ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(23);
    }
    PIT->CHANNEL[0].TFLG = PIT_TFLG_TIF_MASK;
}

void PIT1_IRQHandler(void) {
    PIT->CHANNEL[1].TFLG = PIT_TFLG_TIF_MASK;
    PIT->CHANNEL[1].TCTRL = PIT_TCTRL_TIE_MASK;
    configure_uart_receive();
}

void ADC0_IRQHandler(void) {
    adc_sample = (uint16_t)ADC0->R[0];
    adc_sample_ready = true;
}

void WDOG_EWM_IRQHandler(void) {}

void UART1_ERR_IRQHandler(void) {
    uint8_t status = UART1->S1;

    if ((status & (UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK)) != 0) {
        (void)UART1->D;
    }
}

static void process_uart_frame(void) {
    size_t offset;

    for (offset = 0; offset <= 4; ++offset) {
        if (uart_receive_window[offset] == 0x7b) {
            wqr_protocol_receive(&protocol, uart_receive_window + offset);
            uart_response_due = true;
            return;
        }
    }
}

static void start_uart_response(void) {
    if (!uart_response_due || uart_transmit_active ||
        !wqr_protocol_response(&protocol, uart_transmit_window + UART_FRAME_OFFSET)) {
        return;
    }

    uart_response_due = false;
    uart_transmit_active = true;
    DMA0->CDNE = DMA_CDNE_CDNE(0);
    DMA0->SERQ = DMA_SERQ_SERQ(0);
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

    __disable_irq();
    configure_clock();
    configure_pins();
    prepare_transmit_window();
    configure_uart();
    configure_pit();
    configure_adc();
    configure_spi();
    configure_i2c();
    wqr_protocol_init(&protocol, &io);
    configure_watchdog();
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
        refresh_watchdog();
    }
}
