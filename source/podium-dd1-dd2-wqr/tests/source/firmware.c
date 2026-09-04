#include <cortex_m4_firmware_image.h>
#include <kinetis.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

#define VERIFY_STAGE(name, expression)                                                             \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s failed\n", name);                                                  \
            goto finished;                                                                         \
        }                                                                                          \
    } while (0)

enum {
    RECEIVE_PREFIX_SIZE = 4,
    RECEIVE_WINDOW_SIZE = RECEIVE_PREFIX_SIZE + WQR_FRAME_SIZE,
    TRANSMIT_WINDOW_SIZE = WQR_FRAME_SIZE + 8,
    STARTUP_INSTRUCTIONS = 300000,
    IDLE_INSTRUCTIONS = 50000,
    INTERRUPT_INSTRUCTIONS = 100,
    WATCHDOG_RESET_CYCLES = 1024,
    RESPONSE_INSTRUCTION_LIMIT = 2000000,
    SENSOR_SETTLE_INSTRUCTIONS = 10000000,
    GPIO_PORT_A = 0,
    GPIO_PORT_C = 2,
    SENSOR_SAMPLE = 2048,
    INPUT_FLAGS = 5,
    APPLICATION_BASE = 0xa000,
    SRAM_SIZE = 0x4000,
    ADC0_R0_ADDRESS = 0x4003b010,
    ADC0_CFG1_ADDRESS = 0x4003b008,
    ADC0_CFG2_ADDRESS = 0x4003b00c,
    ADC0_SC2_ADDRESS = 0x4003b020,
    ADC0_SC3_ADDRESS = 0x4003b024,
    ADC0_PG_ADDRESS = 0x4003b02c,
    ADC0_MG_ADDRESS = 0x4003b030,
    MCG_C2_ADDRESS = 0x40064001,
    MCG_C6_ADDRESS = 0x40064005,
    MCG_SC_ADDRESS = 0x40064008,
    PORTA_PCR4_ADDRESS = 0x40049010,
    PORTA_PCR18_ADDRESS = 0x40049048,
    PORTA_PCR19_ADDRESS = 0x4004904c,
    SMC_PMSTAT_ADDRESS = 0x4007e003,
    PMC_REGSC_ADDRESS = 0x4007d002,
    RCM_SRS0_ADDRESS = 0x4007f000,
    RCM_SRS1_ADDRESS = 0x4007f001,
    I2C0_F_ADDRESS = 0x40066001,
    I2C0_C1_ADDRESS = 0x40066002,
    I2C0_FLT_ADDRESS = 0x40066006,
    UART1_S1_ADDRESS = 0x4006b004,
    UART1_C2_ADDRESS = 0x4006b003,
    UART1_C3_ADDRESS = 0x4006b006,
    UART1_C5_ADDRESS = 0x4006b00b,
    UART1_PFIFO_ADDRESS = 0x4006b010,
    UART1_RWFIFO_ADDRESS = 0x4006b015,
    PIT1_LDVAL_ADDRESS = 0x40037110,
    PIT1_TCTRL_ADDRESS = 0x40037118,
    DMA_CR_ADDRESS = 0x40008000,
    SPI0_MCR_ADDRESS = 0x4002c000,
    SPI0_CTAR0_ADDRESS = 0x4002c00c,
    SPI0_POPR_ADDRESS = 0x4002c038,
    WDOG_STCTRLH_ADDRESS = 0x40052000,
    WDOG_TOVALH_ADDRESS = 0x40052004,
    WDOG_TOVALL_ADDRESS = 0x40052006,
    GPIOA_PDDR_ADDRESS = 0x400ff014,
    GPIOC_PDDR_ADDRESS = 0x400ff094,
    SPI_MCR_HALT = 1,
    SPI_MCR_DISABLE_FIFOS = 3u << 12,
    UART_PFIFO_ENABLE_MASK = 0x88,
    INPUT_GPIO_A_MASK = (1u << 4) | (1u << 18) | (1u << 19),
    INPUT_GPIO_C_MASK = 1u << 2,
    SPI_CTAR_CPHA = 1u << 25,
    SPI_CTAR_FMSZ_SHIFT = 27,
    DMA_TCD_BASE = 0x40009000,
    DMA_TCD_STRIDE = 0x20,
    DMA_TCD_ATTR_OFFSET = 0x06,
    DMA_TCD_NBYTES_OFFSET = 0x08,
    DMA_TCD_SLAST_OFFSET = 0x0c,
    DMA_TCD_DOFF_OFFSET = 0x14,
    DMA_TCD_DLAST_OFFSET = 0x18,
    DMA_TCD_CSR_OFFSET = 0x1c,
    DMA_TCD_CITER_OFFSET = 0x16,
    DMA_TCD_BITER_OFFSET = 0x1e,
    DMAMUX_CHCFG0_ADDRESS = 0x40021000,
    DMAMUX_CHCFG1_ADDRESS = 0x40021001,
    DMAMUX_CHCFG2_ADDRESS = 0x40021002,
    DMAMUX_CHCFG3_ADDRESS = 0x40021003,
    DMA_SPI_TRANSMIT_CHANNEL = 2,
    DMA_SPI_RECEIVE_CHANNEL = 3,
    DMA_UART_TRANSMIT_CHANNEL = 0,
    DMA_UART_RECEIVE_CHANNEL = 1,
    DMA0_INTERRUPT = 0,
    DMA3_INTERRUPT = 3,
    SPI0_INTERRUPT = 26,
    UART1_RX_TX_INTERRUPT = 33,
    UART1_ERROR_INTERRUPT = 34,
    PIT0_INTERRUPT = 48,
    EXPECTED_CORE_CLOCK_HZ = 96000000,
    EXPECTED_UART_RESPONSE_GUARD_TICKS = 467,
    EXPECTED_WDOG_CONTROL = 0x40d7,
    EXPECTED_WDOG_TIMEOUT = 0x004c01f4,
    DMA3_PRIORITY = 14,
    PIT0_PRIORITY = 11,
    EXPECTED_UART_RECOVERY_GUARD_TICKS = 3964
};

static const uint32_t NVIC_PRIORITY_BASE = UINT32_C(0xe000e400);
static const uint32_t NVIC_ENABLE_BASE = UINT32_C(0xe000e100);

typedef struct {
    const uint16_t *spi;
    const KinetisI2cTransfer *i2c;
    size_t spi_length;
    size_t spi_index;
    size_t i2c_length;
    size_t i2c_index;
    bool i2c_prefix;
    bool i2c_arbitration_loss;
    bool i2c_nack_on_repeated_start;
    bool spi_receive_overflow;
    bool disconnect_after_spi;
} peripheral_expectations;

static peripheral_expectations expected;

static void expect_spi(const uint16_t *transfers, size_t length) {
    expected.spi = transfers;
    expected.spi_length = length;
    expected.spi_index = 0;
    expected.disconnect_after_spi = false;
}

static void expect_spi_then_disconnect(const uint16_t *transfers, size_t length) {
    expect_spi(transfers, length);
    expected.disconnect_after_spi = true;
}

static void expect_spi_receive_overflow(const uint16_t *transfers, size_t length) {
    expect_spi(transfers, length);
    expected.spi_receive_overflow = true;
}

static void expect_i2c(const KinetisI2cTransfer *transfers, size_t length) {
    expected.i2c = transfers;
    expected.i2c_length = length;
    expected.i2c_index = 0;
    expected.i2c_prefix = false;
}

static void expect_i2c_prefix(const KinetisI2cTransfer *transfers, size_t length) {
    expect_i2c(transfers, length);
    expected.i2c_prefix = true;
}

static void expect_i2c_read(KinetisI2cTransfer *transfers, uint8_t command, size_t length) {
    size_t index = 0;

    transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_START, 0};
    transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_WRITE, 0xa0};
    transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_WRITE, command};
    transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_REPEATED_START, 0};
    transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_WRITE, 0xa1};
    while (length-- != 0) {
        transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_READ, 0};
    }
    transfers[index++] = (KinetisI2cTransfer){KINETIS_I2C_STOP, 0};
    expect_i2c(transfers, index);
}

static bool spi_expectations_met(void) { return expected.spi_index == expected.spi_length; }

static bool i2c_expectations_met(void) { return expected.i2c_index == expected.i2c_length; }

static bool service_spi(Kinetis *device) {
    KinetisSpiTransfer transfer;

    if (!kinetis_spi_transfer(device, KINETIS_SERIAL_SPI0, &transfer)) {
        return true;
    }
    if (expected.spi_index >= expected.spi_length ||
        transfer.data != expected.spi[expected.spi_index]) {
        fprintf(stderr, "unexpected SPI transfer %zu: expected 0x%04x, received 0x%04x\n",
                expected.spi_index,
                expected.spi_index < expected.spi_length ? expected.spi[expected.spi_index] : 0,
                transfer.data);
        return false;
    }
    ++expected.spi_index;
    if (expected.disconnect_after_spi && expected.spi_index == expected.spi_length) {
        expected.disconnect_after_spi = false;
        if (!kinetis_gpio_drive(device, GPIO_PORT_C, 2, true)) {
            return false;
        }
    }
    if (expected.spi_receive_overflow) {
        uint32_t discarded;

        expected.spi_receive_overflow = false;
        return kinetis_read(device, SPI0_POPR_ADDRESS, &discarded, sizeof(discarded)) &&
               kinetis_read(device, SPI0_POPR_ADDRESS, &discarded, sizeof(discarded));
    }
    return true;
}

static bool queue_spi_response(Kinetis *device, const uint8_t *response, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        if (!kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, response[index], 0)) {
            return false;
        }
    }
    return true;
}

static bool service_i2c(Kinetis *device) {
    KinetisI2cTransfer transfer;

    if (!kinetis_i2c_transfer(device, KINETIS_SERIAL_I2C0, &transfer)) {
        return true;
    }
    if (expected.i2c_index >= expected.i2c_length) {
        if (!expected.i2c_prefix) {
            fprintf(stderr, "unexpected I2C transfer %zu: type %u, value 0x%02x\n",
                    expected.i2c_index, (unsigned)transfer.type, transfer.value);
            return false;
        }
    } else if (transfer.type != expected.i2c[expected.i2c_index].type ||
               (transfer.type == KINETIS_I2C_WRITE &&
                transfer.value != expected.i2c[expected.i2c_index].value)) {
        fprintf(stderr, "unexpected I2C transfer %zu: type %u, value 0x%02x\n", expected.i2c_index,
                (unsigned)transfer.type, transfer.value);
        return false;
    }
    if (expected.i2c_index < expected.i2c_length) {
        ++expected.i2c_index;
    }
    if (expected.i2c_arbitration_loss && transfer.type == KINETIS_I2C_START) {
        expected.i2c_arbitration_loss = false;
        return kinetis_i2c_lose_arbitration(device, KINETIS_SERIAL_I2C0);
    }
    if (expected.i2c_nack_on_repeated_start && transfer.type == KINETIS_I2C_REPEATED_START) {
        expected.i2c_nack_on_repeated_start = false;
        return kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, false);
    }
    if (transfer.type == KINETIS_I2C_WRITE) {
        return true;
    }
    if (transfer.type == KINETIS_I2C_READ) {
        return kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, 0x5a);
    }
    return true;
}

static bool step_firmware(Kinetis *device) {
    CortexM4 *cpu = kinetis_cpu(device);
    CortexM4Result result = cortex_m4_step(cpu);
    uint32_t fault_status = cortex_m4_get_fault_status(cpu);

    if (result.stop == CORTEX_M4_STOP_LOCKUP || result.stop == CORTEX_M4_STOP_UNSUPPORTED ||
        result.stop == CORTEX_M4_STOP_BUS_FAULT || result.stop == CORTEX_M4_STOP_USAGE_FAULT ||
        fault_status != 0) {
        fprintf(stderr,
                "firmware stopped: reason %u, fault 0x%08x, PC 0x%08x, LR 0x%08x, SP 0x%08x, "
                "exception %u\n",
                (unsigned)result.stop, fault_status, cortex_m4_get_register(cpu, 15),
                cortex_m4_get_register(cpu, 14), cortex_m4_get_register(cpu, 13),
                (unsigned)(cortex_m4_get_xpsr(cpu) & 0x1ff));
        return false;
    }
    return service_spi(device) && service_i2c(device);
}

static bool run_firmware(Kinetis *device, size_t instructions) {
    while (instructions-- != 0) {
        if (!step_firmware(device)) {
            return false;
        }
    }
    return true;
}

static bool load_firmware(Kinetis *device, const char *path) {
    return cortex_m4_load_elf(device, path, NULL) ||
           cortex_m4_load_binary(device, path, APPLICATION_BASE);
}

static bool register_equals(Kinetis *device, uint32_t address, size_t size,
                            uint32_t expected_value) {
    uint32_t value = 0;

    return kinetis_read(device, address, &value, size) && value == expected_value;
}

static bool watchdog_configuration_valid(Kinetis *device) {
    uint32_t control = 0;
    uint32_t timeout_high = 0;
    uint32_t timeout_low = 0;

    if (!kinetis_read(device, WDOG_STCTRLH_ADDRESS, &control, 2) ||
        !kinetis_read(device, WDOG_TOVALH_ADDRESS, &timeout_high, 2) ||
        !kinetis_read(device, WDOG_TOVALL_ADDRESS, &timeout_low, 2)) {
        return false;
    }
    if (control == EXPECTED_WDOG_CONTROL && timeout_high == EXPECTED_WDOG_TIMEOUT >> 16 &&
        timeout_low == (EXPECTED_WDOG_TIMEOUT & 0xffff)) {
        return true;
    }
    fprintf(stderr, "watchdog mismatch: STCTRLH 0x%04x, TOVALH 0x%04x, TOVALL 0x%04x\n",
            (unsigned)control, (unsigned)timeout_high, (unsigned)timeout_low);
    return false;
}

static bool reference_registers_valid(Kinetis *device) {
    return register_equals(device, MCG_SC_ADDRESS, 1, 0) &&
           register_equals(device, MCG_C2_ADDRESS, 1, 0xa0) &&
           register_equals(device, MCG_C6_ADDRESS, 1, 0) &&
           register_equals(device, PORTA_PCR4_ADDRESS, 4, 0x113) &&
           register_equals(device, PORTA_PCR18_ADDRESS, 4, 0x103) &&
           register_equals(device, PORTA_PCR19_ADDRESS, 4, 0x103) &&
           register_equals(device, UART1_C2_ADDRESS, 1, 0xac) &&
           register_equals(device, UART1_C3_ADDRESS, 1, 0x1f) &&
           register_equals(device, UART1_C5_ADDRESS, 1, 0xa0) &&
           register_equals(device, UART1_RWFIFO_ADDRESS, 1, 0) &&
           register_equals(device, I2C0_C1_ADDRESS, 1, 0xc0) &&
           register_equals(device, ADC0_CFG1_ADDRESS, 4, 0x24) &&
           register_equals(device, ADC0_CFG2_ADDRESS, 4, 0) &&
           register_equals(device, ADC0_SC2_ADDRESS, 4, 0) &&
           register_equals(device, ADC0_SC3_ADDRESS, 4, 7) &&
           register_equals(device, ADC0_PG_ADDRESS, 4, 0x8200) &&
           register_equals(device, ADC0_MG_ADDRESS, 4, 0x8200);
}

static bool uart_descriptors_match_reference(Kinetis *device) {
    uint32_t transmit = DMA_TCD_BASE + DMA_UART_TRANSMIT_CHANNEL * DMA_TCD_STRIDE;
    uint32_t receive = DMA_TCD_BASE + DMA_UART_RECEIVE_CHANNEL * DMA_TCD_STRIDE;
    uint32_t transmit_slast = 0;
    uint32_t receive_slast = 0;
    uint32_t receive_dlast = 0;

    if (!kinetis_read(device, transmit + DMA_TCD_SLAST_OFFSET, &transmit_slast, 4) ||
        !kinetis_read(device, receive + DMA_TCD_SLAST_OFFSET, &receive_slast, 4) ||
        !kinetis_read(device, receive + DMA_TCD_DLAST_OFFSET, &receive_dlast, 4)) {
        return false;
    }
    if (transmit_slast == (uint32_t)-(int32_t)TRANSMIT_WINDOW_SIZE && receive_slast == 0 &&
        receive_dlast == (uint32_t)-(int32_t)RECEIVE_WINDOW_SIZE) {
        return true;
    }
    fprintf(stderr, "UART TCD mismatch: TX SLAST 0x%08x, RX SLAST 0x%08x, DLAST 0x%08x\n",
            transmit_slast, receive_slast, receive_dlast);
    return false;
}

static bool uart_receive_armed(Kinetis *device) {
    uint32_t receive = DMA_TCD_BASE + DMA_UART_RECEIVE_CHANNEL * DMA_TCD_STRIDE;

    return register_equals(device, DMAMUX_CHCFG1_ADDRESS, 1, 0x84) &&
           register_equals(device, receive + DMA_TCD_SLAST_OFFSET, 4, 0) &&
           register_equals(device, receive + DMA_TCD_DLAST_OFFSET, 4,
                           (uint32_t)-(int32_t)RECEIVE_WINDOW_SIZE) &&
           register_equals(device, receive + DMA_TCD_CITER_OFFSET, 2, RECEIVE_WINDOW_SIZE) &&
           register_equals(device, receive + DMA_TCD_BITER_OFFSET, 2, RECEIVE_WINDOW_SIZE);
}

static bool uart_recovery_guard_is_active(Kinetis *device) {
    uint32_t ticks = 0;

    return kinetis_read(device, PIT1_LDVAL_ADDRESS, &ticks, sizeof(ticks)) &&
           ticks > EXPECTED_UART_RESPONSE_GUARD_TICKS &&
           ticks <= EXPECTED_UART_RECOVERY_GUARD_TICKS;
}

static bool wait_for_uart_recovery_guard(Kinetis *device) {
    for (size_t instruction = 0; instruction < INTERRUPT_INSTRUCTIONS * 10; ++instruction) {
        if (uart_recovery_guard_is_active(device)) {
            return true;
        }
        if (!step_firmware(device)) {
            return false;
        }
    }
    return false;
}

static bool startup_uart_descriptor_controls_match_reference(Kinetis *device) {
    uint32_t transmit = DMA_TCD_BASE + DMA_UART_TRANSMIT_CHANNEL * DMA_TCD_STRIDE;
    uint32_t receive = DMA_TCD_BASE + DMA_UART_RECEIVE_CHANNEL * DMA_TCD_STRIDE;

    return register_equals(device, transmit + DMA_TCD_CSR_OFFSET, 2, 0x0a) &&
           register_equals(device, receive + DMA_TCD_CSR_OFFSET, 2, 0x0a);
}

static bool hardware_configuration_valid(Kinetis *device) {
    uint32_t power_mode = 0;
    uint32_t regulator = 0;
    uint32_t i2c_divider = 0;
    uint32_t i2c_filter = 0;
    uint32_t dma_control = 0;
    uint32_t dmamux_transmit = 0;
    uint32_t dmamux_receive = 0;
    uint32_t dmamux_spi_transmit = 0;
    uint32_t dmamux_spi_receive = 0;
    uint32_t spi_control = 0;
    uint32_t uart_fifo = 0;
    uint32_t gpio_a_direction = 0;
    uint32_t gpio_c_direction = 0;
    uint32_t clock = kinetis_core_clock_hz(device);

    bool power_mode_read = kinetis_read(device, SMC_PMSTAT_ADDRESS, &power_mode, 1);
    bool regulator_read = kinetis_read(device, PMC_REGSC_ADDRESS, &regulator, 1);
    bool i2c_divider_read = kinetis_read(device, I2C0_F_ADDRESS, &i2c_divider, 1);
    bool i2c_filter_read = kinetis_read(device, I2C0_FLT_ADDRESS, &i2c_filter, 1);
    bool dma_control_read = kinetis_read(device, DMA_CR_ADDRESS, &dma_control, 4);
    bool dmamux_transmit_read = kinetis_read(device, DMAMUX_CHCFG0_ADDRESS, &dmamux_transmit, 1);
    bool dmamux_receive_read = kinetis_read(device, DMAMUX_CHCFG1_ADDRESS, &dmamux_receive, 1);
    bool dmamux_spi_transmit_read =
        kinetis_read(device, DMAMUX_CHCFG2_ADDRESS, &dmamux_spi_transmit, 1);
    bool dmamux_spi_receive_read =
        kinetis_read(device, DMAMUX_CHCFG3_ADDRESS, &dmamux_spi_receive, 1);
    bool spi_control_read = kinetis_read(device, SPI0_MCR_ADDRESS, &spi_control, 4);
    bool uart_fifo_read = kinetis_read(device, UART1_PFIFO_ADDRESS, &uart_fifo, 1);
    bool gpio_a_direction_read = kinetis_read(device, GPIOA_PDDR_ADDRESS, &gpio_a_direction, 4);
    bool gpio_c_direction_read = kinetis_read(device, GPIOC_PDDR_ADDRESS, &gpio_c_direction, 4);
    if (!power_mode_read || !regulator_read || !i2c_divider_read || !i2c_filter_read ||
        !dma_control_read || !dmamux_transmit_read || !dmamux_receive_read ||
        !dmamux_spi_transmit_read || !dmamux_spi_receive_read || !spi_control_read ||
        !uart_fifo_read || !gpio_a_direction_read || !gpio_c_direction_read) {
        fprintf(stderr, "hardware register read failed at PC 0x%08x\n",
                cortex_m4_get_register(kinetis_cpu(device), 15));
        return false;
    }
    if (clock == EXPECTED_CORE_CLOCK_HZ && power_mode == 0x80 && regulator == 0x04 &&
        i2c_divider == 0x04 && i2c_filter == 0x2a && dma_control == 0 && dmamux_transmit == 0x85 &&
        dmamux_receive == 0x84 && dmamux_spi_transmit == 0x8f && dmamux_spi_receive == 0x8e &&
        (spi_control & (SPI_MCR_DISABLE_FIFOS | SPI_MCR_HALT)) == SPI_MCR_DISABLE_FIFOS &&
        (uart_fifo & UART_PFIFO_ENABLE_MASK) == 0 && (gpio_a_direction & INPUT_GPIO_A_MASK) == 0 &&
        (gpio_c_direction & INPUT_GPIO_C_MASK) == 0) {
        return true;
    }
    fprintf(stderr,
            "invalid hardware configuration: clock %u, PMSTAT 0x%02x, REGSC 0x%02x, "
            "F 0x%02x, FLT 0x%02x, DMA_CR 0x%08x, DMAMUX %02x/%02x/%02x/%02x, "
            "SPI_MCR 0x%08x, PFIFO 0x%02x, "
            "GPIOA_PDDR 0x%08x, GPIOC_PDDR 0x%08x\n",
            clock, (unsigned)power_mode, (unsigned)regulator, (unsigned)i2c_divider,
            (unsigned)i2c_filter, (unsigned)dma_control, (unsigned)dmamux_transmit,
            (unsigned)dmamux_receive, (unsigned)dmamux_spi_transmit, (unsigned)dmamux_spi_receive,
            (unsigned)spi_control, (unsigned)uart_fifo, (unsigned)gpio_a_direction,
            (unsigned)gpio_c_direction);
    return false;
}

static bool spi_dma_is_byte_wide(Kinetis *device) {
    uint32_t transmit = DMA_TCD_BASE + DMA_SPI_TRANSMIT_CHANNEL * DMA_TCD_STRIDE;
    uint32_t receive = DMA_TCD_BASE + DMA_SPI_RECEIVE_CHANNEL * DMA_TCD_STRIDE;

    return register_equals(device, transmit + DMA_TCD_ATTR_OFFSET, 2, 0) &&
           register_equals(device, transmit + DMA_TCD_NBYTES_OFFSET, 4, 1) &&
           register_equals(device, transmit + DMA_TCD_SLAST_OFFSET, 4,
                           (uint32_t)-(int32_t)WQR_SPI_TRANSFER_SIZE) &&
           register_equals(device, transmit + DMA_TCD_DOFF_OFFSET, 2, 0) &&
           register_equals(device, receive + DMA_TCD_ATTR_OFFSET, 2, 0) &&
           register_equals(device, receive + DMA_TCD_NBYTES_OFFSET, 4, 1) &&
           register_equals(device, receive + DMA_TCD_DOFF_OFFSET, 2, 1) &&
           register_equals(device, receive + DMA_TCD_DLAST_OFFSET, 4,
                           (uint32_t)-(int32_t)WQR_SPI_TRANSFER_SIZE);
}

static bool spi_format_is(Kinetis *device, unsigned int bits, bool capture_on_second_edge) {
    uint32_t attributes = 0;

    return kinetis_read(device, SPI0_CTAR0_ADDRESS, &attributes, sizeof(attributes)) &&
           ((attributes >> SPI_CTAR_FMSZ_SHIFT) & 0x0f) == bits - 1 &&
           ((attributes & SPI_CTAR_CPHA) != 0) == capture_on_second_edge;
}

static bool uart_guard_is_exact(Kinetis *device) {
    uint32_t ticks = 0;

    if (!kinetis_read(device, PIT1_LDVAL_ADDRESS, &ticks, sizeof(ticks))) {
        fprintf(stderr, "UART guard register read failed\n");
        return false;
    }
    if (ticks == EXPECTED_UART_RESPONSE_GUARD_TICKS) {
        return true;
    }
    fprintf(stderr, "unexpected UART guard: %u ticks\n", ticks);
    return false;
}

static bool wait_for_uart_error_clear(Kinetis *device) {
    for (size_t instruction = 0; instruction < INTERRUPT_INSTRUCTIONS * 10; ++instruction) {
        uint8_t status = 0;

        if (!kinetis_read(device, UART1_S1_ADDRESS, &status, sizeof(status))) {
            return false;
        }
        if ((status & 0x0f) == 0) {
            return true;
        }
        if (!step_firmware(device)) {
            return false;
        }
    }
    return false;
}

static bool interrupt_priorities_match_reference(Kinetis *device) {
    uint32_t dma_priority = 0;
    uint32_t pit_priority = 0;
    uint32_t spi_priority = 0;
    uint32_t uart_priority = 0;
    uint32_t uart_error_priority = 0;
    uint32_t interrupt_enable[2] = {0};
    CortexM4 *cpu = kinetis_cpu(device);

    return cortex_m4_read_memory(cpu, NVIC_PRIORITY_BASE + DMA3_INTERRUPT, 1, &dma_priority) &&
           cortex_m4_read_memory(cpu, NVIC_PRIORITY_BASE + PIT0_INTERRUPT, 1, &pit_priority) &&
           cortex_m4_read_memory(cpu, NVIC_PRIORITY_BASE + SPI0_INTERRUPT, 1, &spi_priority) &&
           cortex_m4_read_memory(cpu, NVIC_PRIORITY_BASE + UART1_RX_TX_INTERRUPT, 1,
                                 &uart_priority) &&
           cortex_m4_read_memory(cpu, NVIC_PRIORITY_BASE + UART1_ERROR_INTERRUPT, 1,
                                 &uart_error_priority) &&
           cortex_m4_read_memory(cpu, NVIC_ENABLE_BASE, sizeof(interrupt_enable[0]),
                                 &interrupt_enable[0]) &&
           cortex_m4_read_memory(cpu, NVIC_ENABLE_BASE + sizeof(interrupt_enable[0]),
                                 sizeof(interrupt_enable[1]), &interrupt_enable[1]) &&
           dma_priority == DMA3_PRIORITY << 4 && pit_priority == PIT0_PRIORITY << 4 &&
           spi_priority == 0 && uart_priority == 0 && uart_error_priority == 0 &&
           (interrupt_enable[SPI0_INTERRUPT / 32] & (UINT32_C(1) << (SPI0_INTERRUPT % 32))) != 0 &&
           (interrupt_enable[UART1_RX_TX_INTERRUPT / 32] &
            (UINT32_C(1) << (UART1_RX_TX_INTERRUPT % 32))) == 0 &&
           (interrupt_enable[UART1_ERROR_INTERRUPT / 32] &
            (UINT32_C(1) << (UART1_ERROR_INTERRUPT % 32))) != 0;
}

static bool send_uart(Kinetis *device, const uint8_t *data, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        while (!kinetis_uart1_receive(device, data[index], 0)) {
            if (!step_firmware(device)) {
                return false;
            }
        }
    }
    return true;
}

static bool send_request(Kinetis *device, const uint8_t frame[WQR_FRAME_SIZE]) {
    uint8_t window[RECEIVE_WINDOW_SIZE] = {0};

    memcpy(window + RECEIVE_PREFIX_SIZE, frame, WQR_FRAME_SIZE);
    return send_uart(device, window, sizeof(window));
}

static bool receive_response_from(Kinetis *device, uint8_t window[TRANSMIT_WINDOW_SIZE],
                                  size_t length) {
    for (size_t instruction = 0; instruction < RESPONSE_INSTRUCTION_LIMIT; ++instruction) {
        uint8_t value;

        while (length < TRANSMIT_WINDOW_SIZE && kinetis_uart1_transmit(device, &value)) {
            window[length++] = value;
        }
        if (length == TRANSMIT_WINDOW_SIZE) {
            return true;
        }
        if (!step_firmware(device)) {
            return false;
        }
    }
    return false;
}

static bool startup_spi_descriptors_match_reference(Kinetis *device) {
    uint32_t transmit = DMA_TCD_BASE + DMA_SPI_TRANSMIT_CHANNEL * DMA_TCD_STRIDE;
    uint32_t receive = DMA_TCD_BASE + DMA_SPI_RECEIVE_CHANNEL * DMA_TCD_STRIDE;

    return spi_dma_is_byte_wide(device) &&
           register_equals(device, transmit + DMA_TCD_CITER_OFFSET, 2, WQR_SPI_TRANSFER_SIZE) &&
           register_equals(device, transmit + DMA_TCD_CSR_OFFSET, 2, 0x0a) &&
           register_equals(device, transmit + DMA_TCD_BITER_OFFSET, 2, WQR_SPI_TRANSFER_SIZE) &&
           register_equals(device, receive + DMA_TCD_CITER_OFFSET, 2, WQR_SPI_TRANSFER_SIZE) &&
           register_equals(device, receive + DMA_TCD_CSR_OFFSET, 2, 0x0a) &&
           register_equals(device, receive + DMA_TCD_BITER_OFFSET, 2, WQR_SPI_TRANSFER_SIZE);
}

static bool receive_response(Kinetis *device, uint8_t window[TRANSMIT_WINDOW_SIZE]) {
    return receive_response_from(device, window, 0);
}

static bool exchange_frame(Kinetis *device, uint8_t type, uint8_t sequence, const uint8_t *payload,
                           size_t payload_length, uint8_t response[TRANSMIT_WINDOW_SIZE]) {
    uint8_t request[WQR_FRAME_SIZE];

    return wqr_frame_build(request, type, sequence, payload, payload_length) &&
           send_request(device, request) && receive_response(device, response);
}

static bool frame_integrity_valid(const uint8_t frame[WQR_FRAME_SIZE]) {
    uint16_t crc = (uint16_t)frame[61] | (uint16_t)((uint16_t)frame[62] << 8);

    return frame[0] == 0x7b && frame[63] == 0x7d &&
           wqr_frame_crc(frame + 1, WQR_FRAME_BODY_SIZE) == crc;
}

static bool status_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE], uint8_t sequence,
                                  uint8_t command_marker) {
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;

    return frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_STATUS && frame[2] == sequence &&
           frame[3] == WQR_STATUS_SIZE && frame[4] == 7 && frame[18] == command_marker;
}

static bool nack_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE]) {
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;

    return frame_integrity_valid(frame) && frame[1] == 0;
}

static bool exchange_status(Kinetis *device, uint8_t request_sequence, uint8_t command_marker) {
    uint8_t response[TRANSMIT_WINDOW_SIZE] = {0};
    const uint8_t *payload = command_marker == 0 ? NULL : &command_marker;
    size_t payload_length = command_marker == 0 ? 0 : 1;

    if (!exchange_frame(device, WQR_PAYLOAD_STATUS, request_sequence, payload, payload_length,
                        response)) {
        fprintf(stderr, "status response timed out\n");
        return false;
    }
    if (!status_response_valid(response, (uint8_t)(request_sequence + 1), command_marker)) {
        fprintf(stderr,
                "invalid status response: type 0x%02x sequence %u length %u marker 0x%02x\n",
                response[RECEIVE_PREFIX_SIZE + 1], response[RECEIVE_PREFIX_SIZE + 2],
                response[RECEIVE_PREFIX_SIZE + 3], response[RECEIVE_PREFIX_SIZE + 18]);
        return false;
    }
    return true;
}

static bool exchange_measured_status(Kinetis *device, uint8_t request_sequence) {
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;
    uint16_t sample;
    uint16_t sensor_value;

    if (!kinetis_read(device, ADC0_R0_ADDRESS, &sample, sizeof(sample))) {
        return false;
    }
    sensor_value = (uint16_t)wqr_sensor_value(sample);
    return exchange_frame(device, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0, response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), 0) &&
           frame[5] == INPUT_FLAGS && frame[6] == (uint8_t)sensor_value &&
           frame[7] == (uint8_t)(sensor_value >> 8);
}

static bool exchange_input_status(Kinetis *device, uint8_t request_sequence, uint8_t inputs) {
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    return exchange_frame(device, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0, response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), 0) &&
           frame[5] == inputs;
}

static bool exchange_shifted_status(Kinetis *device, uint8_t request_sequence) {
    uint8_t request[RECEIVE_WINDOW_SIZE] = {0};
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    return wqr_frame_build(request, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0) &&
           send_uart(device, request, sizeof(request)) && receive_response(device, response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), 0);
}

static bool verify_ack_recovery(Kinetis *device, uint8_t sequence) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    uint8_t unexpected;

    if (!wqr_frame_build(request, 1, sequence, NULL, 0) || !send_request(device, request) ||
        !run_firmware(device, IDLE_INSTRUCTIONS) || kinetis_uart1_transmit(device, &unexpected)) {
        return false;
    }
    return exchange_frame(device, 1, sequence, NULL, 0, response) &&
           nack_response_valid(response) && run_firmware(device, IDLE_INSTRUCTIONS) &&
           exchange_status(device, (uint8_t)(sequence + 1), 0);
}

static bool recover_from_noise(Kinetis *device) {
    uint8_t noise[WQR_FRAME_SIZE] = {0};
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    return send_request(device, noise) && receive_response(device, response) &&
           nack_response_valid(response);
}

static bool recover_from_uart_error(Kinetis *device, uint8_t request_sequence) {
    const uint8_t errors[] = {1, 2, 4, 8, 0x0f};

    for (size_t index = 0; index < sizeof(errors); ++index) {
        uint32_t status = 0;

        if (!kinetis_uart1_error(device, errors[index]) || !wait_for_uart_error_clear(device) ||
            !run_firmware(device, INTERRUPT_INSTRUCTIONS) ||
            !kinetis_read(device, UART1_S1_ADDRESS, &status, 1) || (status & 0x0f) != 0) {
            return false;
        }
    }
    return exchange_status(device, request_sequence, 0);
}

static bool recover_from_bad_end_marker(Kinetis *device) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    if (!wqr_frame_build(request, WQR_PAYLOAD_STATUS, 1, NULL, 0)) {
        return false;
    }
    request[WQR_FRAME_SIZE - 1] = 0;
    if (!send_request(device, request) || !wait_for_uart_recovery_guard(device)) {
        return false;
    }
    if (!uart_receive_armed(device)) {
        return false;
    }
    return receive_response(device, response) && nack_response_valid(response);
}

static bool reject_invalid_crc(Kinetis *device, uint8_t request_sequence) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    if (!wqr_frame_build(request, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0)) {
        return false;
    }
    request[61] ^= 1;
    return send_request(device, request) && receive_response(device, response) &&
           nack_response_valid(response);
}

static bool reject_oversized_payload(Kinetis *device, uint8_t request_sequence) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    uint16_t crc;

    if (!wqr_frame_build(request, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0)) {
        return false;
    }
    request[3] = WQR_FRAME_PAYLOAD_SIZE + 1;
    crc = wqr_frame_crc(request + 1, WQR_FRAME_BODY_SIZE);
    request[61] = (uint8_t)crc;
    request[62] = (uint8_t)(crc >> 8);
    return send_request(device, request) && receive_response(device, response) &&
           nack_response_valid(response);
}

static bool reject_invalid_payload_type(Kinetis *device, uint8_t request_sequence) {
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    return exchange_frame(device, 6, request_sequence, NULL, 0, response) &&
           nack_response_valid(response);
}

static bool reject_fragment_type_change(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x10};
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    if (!exchange_frame(device, 0x10 | WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                        response) ||
        !frame_integrity_valid(frame) || frame[1] != 1 || frame[4] != WQR_PAYLOAD_STATUS) {
        return false;
    }
    return exchange_frame(device, 0x40 | WQR_PAYLOAD_STATUS, (uint8_t)(request_sequence + 1), NULL,
                          0, response) &&
           nack_response_valid(response) && frame[4] == 1;
}

static bool primary_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE], uint8_t sequence,
                                   const uint8_t payload[WQR_FRAME_PAYLOAD_SIZE], bool exchanged) {
    static const uint8_t empty_response[WQR_SPI_TRANSFER_SIZE] = {0};
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;
    const uint8_t *expected_response = exchanged ? payload : empty_response;

    return frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_PRIMARY_SPI &&
           frame[2] == sequence && frame[3] == WQR_FRAME_PAYLOAD_SIZE && (frame[60] & 2) != 0 &&
           memcmp(frame + 4, expected_response, WQR_SPI_TRANSFER_SIZE) == 0;
}

static bool exchange_primary_spi(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    bool transfer_control = false;

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(index + 1);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(NULL, 0);
    if (!kinetis_gpio_drive(device, GPIO_PORT_C, 2, false) ||
        !exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload, sizeof(payload),
                        response) ||
        !primary_response_valid(response, (uint8_t)(request_sequence + 1), payload, false) ||
        !spi_expectations_met() || !kinetis_gpio_pin(device, GPIO_PORT_C, 3, &transfer_control) ||
        !transfer_control || !run_firmware(device, IDLE_INSTRUCTIONS) ||
        !queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE)) {
        return false;
    }
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    return exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, (uint8_t)(request_sequence + 1), payload,
                          sizeof(payload), response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 2), payload, true) &&
           spi_expectations_met();
}

static bool exchange_repeated_primary_spi(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(WQR_SPI_TRANSFER_SIZE - index);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    return queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE) &&
           exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload,
                          sizeof(payload), response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 1), payload, true) &&
           spi_expectations_met();
}

static bool exchange_primary_spi_after_retry(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    uint32_t control = 0;
    bool transfer_active = true;

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(0x40 + index);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    if (!queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE) ||
        !wqr_frame_build(request, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload,
                         sizeof(payload)) ||
        !send_request(device, request)) {
        return false;
    }
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    for (size_t instruction = 0; instruction < RESPONSE_INSTRUCTION_LIMIT; ++instruction) {
        if (!kinetis_gpio_pin(device, GPIO_PORT_C, 4, &transfer_active)) {
            return false;
        }
        if (!transfer_active) {
            break;
        }
        if (!step_firmware(device)) {
            return false;
        }
    }
    if (transfer_active || !kinetis_read(device, SPI0_MCR_ADDRESS, &control, sizeof(control))) {
        return false;
    }
    control |= SPI_MCR_HALT;
    return kinetis_write(device, SPI0_MCR_ADDRESS, &control, sizeof(control)) &&
           receive_response(device, response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 1), payload, true) &&
           spi_expectations_met();
}

static bool exchange_after_reconnect(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE] = {0};
    uint8_t disconnected_response[TRANSMIT_WINDOW_SIZE] = {0};
    uint8_t unexpected;
    bool transfer_control = false;
    bool transfer_active = true;

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(0xa0 + index);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi_then_disconnect(transmitted, WQR_SPI_TRANSFER_SIZE);
    if (!wqr_frame_build(request, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload,
                         sizeof(payload)) ||
        !send_request(device, request)) {
        fprintf(stderr, "SPI reconnect request setup failed\n");
        return false;
    }
    for (size_t instruction = 0; instruction < RESPONSE_INSTRUCTION_LIMIT; ++instruction) {
        if (!kinetis_gpio_pin(device, GPIO_PORT_C, 4, &transfer_active)) {
            return false;
        }
        if (!transfer_active && spi_expectations_met()) {
            break;
        }
        if (!step_firmware(device)) {
            return false;
        }
    }
    if (transfer_active || !spi_expectations_met()) {
        fprintf(stderr, "SPI reconnect transfer did not complete\n");
        return false;
    }
    if (!run_firmware(device, IDLE_INSTRUCTIONS)) {
        fprintf(stderr, "SPI reconnect disconnect execution failed\n");
        return false;
    }
    if (kinetis_uart1_transmit(device, &unexpected)) {
        disconnected_response[0] = unexpected;
        if (!receive_response_from(device, disconnected_response, 1) ||
            !frame_integrity_valid(disconnected_response + RECEIVE_PREFIX_SIZE) ||
            !run_firmware(device, IDLE_INSTRUCTIONS) ||
            kinetis_uart1_transmit(device, &unexpected)) {
            fprintf(stderr, "SPI reconnect left an invalid or repeated disconnect response\n");
            return false;
        }
    }
    if (!kinetis_gpio_pin(device, GPIO_PORT_C, 3, &transfer_control) || !transfer_control ||
        !startup_spi_descriptors_match_reference(device)) {
        fprintf(stderr, "SPI reconnect transfer control did not reset\n");
        return false;
    }
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    if (!kinetis_gpio_drive(device, GPIO_PORT_C, 2, false) ||
        !run_firmware(device, IDLE_INSTRUCTIONS) ||
        !queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE) ||
        !exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, (uint8_t)(request_sequence + 1), payload,
                        sizeof(payload), response) ||
        !primary_response_valid(response, (uint8_t)(request_sequence + 2), payload, true)) {
        fprintf(stderr, "SPI reconnect first exchange failed\n");
        return false;
    }
    if (!run_firmware(device, IDLE_INSTRUCTIONS) ||
        !queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE)) {
        fprintf(stderr, "SPI reconnect repeat setup failed\n");
        return false;
    }
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    if (!exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, (uint8_t)(request_sequence + 2), payload,
                        sizeof(payload), response) ||
        !primary_response_valid(response, (uint8_t)(request_sequence + 3), payload, true) ||
        !spi_expectations_met()) {
        fprintf(stderr, "SPI reconnect repeated exchange failed\n");
        return false;
    }
    return true;
}

static bool exchange_alternate_spi_word(Kinetis *device, uint8_t request_sequence,
                                        uint16_t request_word, uint16_t transmitted_word,
                                        uint16_t received_word) {
    const uint16_t transmitted[] = {transmitted_word};
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {(uint8_t)request_word, (uint8_t)(request_word >> 8)};
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(transmitted, 1);
    return kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, received_word, 0) &&
           exchange_frame(device, WQR_PAYLOAD_ALTERNATE_SPI, request_sequence, payload,
                          sizeof(payload), response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_ALTERNATE_SPI &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == WQR_FRAME_PAYLOAD_SIZE &&
           frame[4] == (uint8_t)received_word && frame[5] == (uint8_t)(received_word >> 8) &&
           (frame[60] & 2) != 0 && spi_expectations_met();
}

static bool exchange_alternate_spi(Kinetis *device, uint8_t request_sequence) {
    return exchange_alternate_spi_word(device, request_sequence, UINT16_C(0x1234), 0, 0) &&
           run_firmware(device, IDLE_INSTRUCTIONS) &&
           exchange_alternate_spi_word(device, (uint8_t)(request_sequence + 1), UINT16_C(0x1234),
                                       UINT16_C(0x1234), UINT16_C(0x1234));
}

static bool exchange_alternate_spi_error(Kinetis *device, uint8_t request_sequence) {
    const uint16_t transmitted[] = {UINT16_C(0x5678)};
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0x78, 0x56};
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi_receive_overflow(transmitted, 1);
    return kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, UINT16_C(0xbeef), 0) &&
           exchange_frame(device, WQR_PAYLOAD_ALTERNATE_SPI, request_sequence, payload,
                          sizeof(payload), response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_ALTERNATE_SPI &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == WQR_FRAME_PAYLOAD_SIZE &&
           (frame[4] != 0xef || frame[5] != 0xbe) && (frame[60] & 2) != 0 &&
           spi_expectations_met() && !expected.spi_receive_overflow;
}

static bool exchange_alternate_spi_recovery(Kinetis *device, uint8_t request_sequence) {
    return exchange_alternate_spi_word(device, request_sequence, UINT16_C(0xabcd), UINT16_C(0xabcd),
                                       UINT16_C(0xabcd));
}

static bool reconnect_after_alternate_spi(Kinetis *device) {
    bool transfer_control = false;

    expect_spi(NULL, 0);
    return kinetis_gpio_drive(device, GPIO_PORT_C, 2, true) &&
           run_firmware(device, IDLE_INSTRUCTIONS) && spi_expectations_met() &&
           spi_format_is(device, 8, true) &&
           kinetis_gpio_pin(device, GPIO_PORT_C, 3, &transfer_control) && transfer_control &&
           kinetis_gpio_drive(device, GPIO_PORT_C, 2, false) &&
           run_firmware(device, IDLE_INSTRUCTIONS) && spi_format_is(device, 8, true);
}

static bool exchange_i2c_write(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa0, 0x10, 0x22};
    const KinetisI2cTransfer transfers[] = {
        {KINETIS_I2C_START, 0},    {KINETIS_I2C_WRITE, 0xa0}, {KINETIS_I2C_WRITE, 0x10},
        {KINETIS_I2C_WRITE, 0x22}, {KINETIS_I2C_STOP, 0},
    };
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c(transfers, sizeof(transfers) / sizeof(transfers[0]));
    return exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                          response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 1 &&
           frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met();
}

static bool exchange_i2c_nack(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa0, 0x10, 0x22};
    const KinetisI2cTransfer transfers[] = {
        {KINETIS_I2C_START, 0},
        {KINETIS_I2C_WRITE, 0xa0},
    };
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_prefix(transfers, sizeof(transfers) / sizeof(transfers[0]));
    bool valid = kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, false) &&
                 exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                                response) &&
                 frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
                 frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 0 &&
                 frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met();
    return kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, true) && valid;
}

static bool exchange_i2c_read_nack(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x10, 3, 0};
    KinetisI2cTransfer transfers[6];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_read(transfers, 0x10, 0);
    expected.i2c_nack_on_repeated_start = true;
    bool valid = exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                                response) &&
                 frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
                 frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 0 &&
                 frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met() &&
                 !expected.i2c_nack_on_repeated_start;
    return kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, true) && valid;
}

static bool exchange_i2c_arbitration_loss(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa0, 0x10, 0x22};
    const KinetisI2cTransfer transfers[] = {{KINETIS_I2C_START, 0}};
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c(transfers, sizeof(transfers) / sizeof(transfers[0]));
    expected.i2c_arbitration_loss = true;
    return exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                          response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 0 &&
           frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met() &&
           !expected.i2c_arbitration_loss;
}

static bool exchange_i2c_timeout(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa0, 0x10, 0x22};
    const uint8_t slow_clock = 0xff;
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_prefix(NULL, 0);
    return kinetis_write(device, I2C0_F_ADDRESS, &slow_clock, sizeof(slow_clock)) &&
           exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                          response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 0 &&
           frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met();
}

static bool exchange_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x10, 3, 0};
    KinetisI2cTransfer transfers[9];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_read(transfers, 0x10, 3);
    return exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                          response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 5 && frame[4] == 1 &&
           frame[5] == payload[1] && frame[6] == 0x5a && frame[7] == 0x5a && frame[8] == 0x5a &&
           i2c_expectations_met();
}

static bool exchange_empty_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x10, 0, 0};
    KinetisI2cTransfer transfers[7];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_read(transfers, 0x10, 1);
    return exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                          response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 2 && frame[4] == 1 &&
           frame[5] == payload[1] && i2c_expectations_met();
}

static bool exchange_fragmented_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t first_payload[] = {0, 0xa1, 0x10};
    const uint8_t last_payload[] = {3, 0};
    KinetisI2cTransfer transfers[9];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    if (!exchange_frame(device, 0x10 | WQR_PAYLOAD_I2C, request_sequence, first_payload,
                        sizeof(first_payload), response) ||
        !frame_integrity_valid(frame) || frame[1] != 1 || frame[4] != WQR_PAYLOAD_I2C) {
        return false;
    }
    expect_i2c_read(transfers, 0x10, 3);
    return run_firmware(device, IDLE_INSTRUCTIONS) &&
           exchange_frame(device, 0x40 | WQR_PAYLOAD_I2C, (uint8_t)(request_sequence + 1),
                          last_payload, sizeof(last_payload), response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 2) && frame[3] == 5 && frame[4] == 1 &&
           frame[5] == 0xa1 && frame[6] == 0x5a && frame[7] == 0x5a && frame[8] == 0x5a &&
           i2c_expectations_met();
}

static bool exchange_chunked_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x20, 120, 0};
    KinetisI2cTransfer transfers[126];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_read(transfers, 0x20, 120);
    if (!exchange_frame(device, WQR_PAYLOAD_I2C, request_sequence, payload, sizeof(payload),
                        response) ||
        !frame_integrity_valid(frame) || frame[1] != (0x10 | WQR_PAYLOAD_I2C) ||
        frame[2] != (uint8_t)(request_sequence + 1) || frame[3] != WQR_FRAME_PAYLOAD_SIZE ||
        !i2c_expectations_met()) {
        return false;
    }
    if (!run_firmware(device, IDLE_INSTRUCTIONS) ||
        !exchange_frame(device, 1, (uint8_t)(request_sequence + 1), NULL, 0, response) ||
        !frame_integrity_valid(frame) || frame[1] != (0x20 | WQR_PAYLOAD_I2C) ||
        frame[2] != (uint8_t)(request_sequence + 2) || frame[3] != WQR_FRAME_PAYLOAD_SIZE) {
        return false;
    }
    return run_firmware(device, IDLE_INSTRUCTIONS) &&
           exchange_frame(device, 1, (uint8_t)(request_sequence + 2), NULL, 0, response) &&
           frame_integrity_valid(frame) && frame[1] == (0x40 | WQR_PAYLOAD_I2C) &&
           frame[2] == (uint8_t)(request_sequence + 3) && frame[3] == 8 && frame[4] == 0x5a &&
           frame[5] == 0x5a && frame[6] == 0x5a && frame[7] == 0x5a && frame[8] == 0x5a;
}

static bool configure_inputs(Kinetis *device, uint8_t inputs) {
    return kinetis_set_adc_input(device, 0, KINETIS_ADC_MUX_A, 23, SENSOR_SAMPLE) &&
           kinetis_gpio_drive(device, GPIO_PORT_A, 4, (inputs & 1) == 0) &&
           kinetis_gpio_drive(device, GPIO_PORT_A, 18, (inputs & 2) == 0) &&
           kinetis_gpio_drive(device, GPIO_PORT_A, 19, (inputs & 4) == 0);
}

static bool reset_recorded(Kinetis *device, uint32_t address, uint8_t source) {
    uint8_t reset_status;

    return kinetis_read(device, address, &reset_status, sizeof(reset_status)) &&
           (reset_status & source) != 0;
}

static bool ignore_stale_spi_completion(Kinetis *device) {
    CortexM4 *cpu = kinetis_cpu(device);

    cortex_m4_set_irq(cpu, DMA3_INTERRUPT, true);
    return run_firmware(device, INTERRUPT_INSTRUCTIONS) &&
           !cortex_m4_get_irq_pending(cpu, DMA3_INTERRUPT);
}

static bool ignore_stale_uart_completion(Kinetis *device) {
    CortexM4 *cpu = kinetis_cpu(device);
    uint32_t timer_control = 0;

    cortex_m4_set_irq(cpu, DMA0_INTERRUPT, true);
    return run_firmware(device, INTERRUPT_INSTRUCTIONS) &&
           !cortex_m4_get_irq_pending(cpu, DMA0_INTERRUPT) &&
           kinetis_read(device, PIT1_TCTRL_ADDRESS, &timer_control, sizeof(timer_control)) &&
           (timer_control & 1) == 0;
}

static bool firmware_passes(const char *path, const char *elf_path, KinetisPackage package) {
    KinetisConfiguration configuration = kinetis_configuration(KINETIS_PROFILE_MKV30F12810);
    CortexM4CoverageResult coverage_result;
    CortexM4Coverage *coverage;
    Kinetis *device;
    uint64_t uninitialized_reads;
    bool passed = false;

    configuration.package = package;
    configuration.vector_table_address = APPLICATION_BASE;
    configuration.sram_size = SRAM_SIZE;
    device = kinetis_create(configuration);
    if (device == NULL) {
        return false;
    }
    coverage = cortex_m4_coverage_create_elf(elf_path);
    if (coverage == NULL) {
        kinetis_destroy(device);
        return false;
    }
    cortex_m4_set_coverage(kinetis_cpu(device), coverage);
    VERIFY_STAGE("load firmware",
                 configure_inputs(device, INPUT_FLAGS) && load_firmware(device, path));
    VERIFY_STAGE("start firmware", kinetis_reset(device) &&
                                       kinetis_set_reset_state(device, 1, true) &&
                                       run_firmware(device, STARTUP_INSTRUCTIONS));
    VERIFY_STAGE("hardware configuration", hardware_configuration_valid(device));
    VERIFY_STAGE("watchdog configuration", watchdog_configuration_valid(device));
    VERIFY_STAGE("reference registers", reference_registers_valid(device));
    VERIFY_STAGE("interrupt priorities", interrupt_priorities_match_reference(device));
    VERIFY_STAGE("startup UART descriptors",
                 uart_descriptors_match_reference(device) &&
                     startup_uart_descriptor_controls_match_reference(device));
    VERIFY_STAGE("startup SPI descriptors", startup_spi_descriptors_match_reference(device));
    VERIFY_STAGE("status", exchange_status(device, 0, 0));
    VERIFY_STAGE("UART response guard", run_firmware(device, INTERRUPT_INSTRUCTIONS * 10) &&
                                            uart_guard_is_exact(device) &&
                                            uart_descriptors_match_reference(device));
    VERIFY_STAGE("measured status", run_firmware(device, SENSOR_SETTLE_INSTRUCTIONS) &&
                                        exchange_measured_status(device, 1));
    VERIFY_STAGE("stale UART completion",
                 run_firmware(device, IDLE_INSTRUCTIONS) && ignore_stale_uart_completion(device));
    VERIFY_STAGE("UART noise recovery",
                 run_firmware(device, IDLE_INSTRUCTIONS) && recover_from_noise(device));
    VERIFY_STAGE("UART recovery turnaround", exchange_status(device, 2, 0));
    VERIFY_STAGE("UART error recovery", recover_from_uart_error(device, 3));
    VERIFY_STAGE("UART framing recovery",
                 run_firmware(device, IDLE_INSTRUCTIONS) && recover_from_bad_end_marker(device));
    VERIFY_STAGE("CRC rejection",
                 run_firmware(device, IDLE_INSTRUCTIONS) && reject_invalid_crc(device, 2));
    VERIFY_STAGE("payload length rejection",
                 run_firmware(device, IDLE_INSTRUCTIONS) && reject_oversized_payload(device, 2));
    VERIFY_STAGE("payload type rejection",
                 run_firmware(device, IDLE_INSTRUCTIONS) && reject_invalid_payload_type(device, 2));
    VERIFY_STAGE("fragment type rejection",
                 run_firmware(device, IDLE_INSTRUCTIONS) && reject_fragment_type_change(device, 2));
    VERIFY_STAGE("stale SPI completion", ignore_stale_spi_completion(device));
    VERIFY_STAGE("primary SPI", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                    exchange_primary_spi(device, 3) &&
                                    spi_dma_is_byte_wide(device) && spi_format_is(device, 8, true));
    VERIFY_STAGE("repeated primary SPI", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                             exchange_repeated_primary_spi(device, 5));
    VERIFY_STAGE("SPI retry", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                  exchange_primary_spi_after_retry(device, 6));
    VERIFY_STAGE("SPI reconnect",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_after_reconnect(device, 7) &&
                     spi_dma_is_byte_wide(device) && spi_format_is(device, 8, true));
    VERIFY_STAGE("alternate SPI", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                      exchange_alternate_spi(device, 10) &&
                                      spi_format_is(device, 16, false));
    VERIFY_STAGE("alternate SPI error", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                            exchange_alternate_spi_error(device, 12));
    VERIFY_STAGE("alternate SPI recovery", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                               exchange_alternate_spi_recovery(device, 13));
    VERIFY_STAGE("alternate SPI reconnect",
                 run_firmware(device, IDLE_INSTRUCTIONS) && reconnect_after_alternate_spi(device));
    VERIFY_STAGE("I2C write",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_write(device, 15));
    VERIFY_STAGE("I2C NACK",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_nack(device, 16));
    VERIFY_STAGE("I2C read NACK",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_read_nack(device, 17));
    VERIFY_STAGE("I2C arbitration loss", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                             exchange_i2c_arbitration_loss(device, 18));
    VERIFY_STAGE("I2C timeout",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_timeout(device, 19));
    VERIFY_STAGE("empty I2C read",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_empty_i2c_read(device, 20));
    VERIFY_STAGE("I2C read",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_read(device, 21));
    VERIFY_STAGE("fragmented I2C read", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                            exchange_fragmented_i2c_read(device, 22));
    VERIFY_STAGE("chunked I2C response",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_chunked_i2c_read(device, 24));
    VERIFY_STAGE("software reset request", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                               exchange_status(device, 27, 0xaa) &&
                                               run_firmware(device, STARTUP_INSTRUCTIONS) &&
                                               reset_recorded(device, RCM_SRS1_ADDRESS, 4));
    VERIFY_STAGE("status after reset",
                 exchange_status(device, 0, 0) && hardware_configuration_valid(device) &&
                     watchdog_configuration_valid(device) && reference_registers_valid(device));
    VERIFY_STAGE("input snapshot",
                 configure_inputs(device, 2) && exchange_input_status(device, 1, INPUT_FLAGS));
    VERIFY_STAGE("shifted UART frame",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_shifted_status(device, 2));
    VERIFY_STAGE("ACK recovery",
                 run_firmware(device, IDLE_INSTRUCTIONS) && verify_ack_recovery(device, 3));
    kinetis_watchdog_advance(device, EXPECTED_WDOG_TIMEOUT);
    kinetis_advance(device, WATCHDOG_RESET_CYCLES);
    VERIFY_STAGE("watchdog reset source", reset_recorded(device, RCM_SRS0_ADDRESS, 0x20));
    VERIFY_STAGE("watchdog restart", run_firmware(device, STARTUP_INSTRUCTIONS));
    VERIFY_STAGE("watchdog recovery", hardware_configuration_valid(device) &&
                                          watchdog_configuration_valid(device) &&
                                          exchange_status(device, 0, 0));
    passed = true;

finished:
    coverage_result = cortex_m4_coverage_result(coverage);
    uninitialized_reads = kinetis_get_uninitialized_sram_read_count(device);
    passed = passed && coverage_result.outside_range == 0 && uninitialized_reads == 0;
    printf("%s: instructions %zu/%zu (%.2f%%), branches %zu/%zu (%.2f%%), "
           "%llu executed, %llu invalid reads\n",
           path, coverage_result.covered_instructions, coverage_result.total_instructions,
           coverage_result.instruction_coverage_percent, coverage_result.covered_branch_sites,
           coverage_result.total_branch_sites, coverage_result.branch_coverage_percent,
           (unsigned long long)coverage_result.instructions,
           (unsigned long long)uninitialized_reads);
    cortex_m4_set_coverage(kinetis_cpu(device), NULL);
    cortex_m4_coverage_destroy(coverage);
    kinetis_destroy(device);
    return passed;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    return firmware_passes(argv[1], argv[2], KINETIS_PACKAGE_LH_64_LQFP) ? EXIT_SUCCESS
                                                                         : EXIT_FAILURE;
}
