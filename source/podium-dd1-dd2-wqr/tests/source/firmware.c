#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cortex_m4_firmware_image.h"
#include "kinetis.h"
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
    RESPONSE_INSTRUCTION_LIMIT = 1000000,
    SENSOR_SETTLE_INSTRUCTIONS = 10000000,
    GPIO_PORT_A = 0,
    GPIO_PORT_C = 2,
    SENSOR_SAMPLE = 2048,
    INPUT_FLAGS = 5,
    APPLICATION_BASE = 0xa000,
    SRAM_SIZE = 0x4000,
    FLASH_SIZE = 0x20000,
    SMC_PMSTAT_ADDRESS = 0x4007e003,
    PMC_REGSC_ADDRESS = 0x4007d002,
    I2C0_FLT_ADDRESS = 0x40066006,
    PIT1_LDVAL_ADDRESS = 0x40037110,
    SPI0_CTAR0_ADDRESS = 0x4002c00c,
    SPI_CTAR_CPHA = 1u << 25,
    SPI_CTAR_FMSZ_SHIFT = 27,
    DMA_TCD_BASE = 0x40009000,
    DMA_TCD_STRIDE = 0x20,
    DMA_TCD_ATTR_OFFSET = 0x06,
    DMA_TCD_NBYTES_OFFSET = 0x08,
    DMA_TCD_DOFF_OFFSET = 0x14,
    DMA_SPI_TRANSMIT_CHANNEL = 2,
    DMA_SPI_RECEIVE_CHANNEL = 3,
    EXPECTED_CORE_CLOCK_HZ = 96000000,
    EXPECTED_UART_RESPONSE_GUARD_TICKS = 467
};

typedef struct {
    const uint16_t *spi;
    const KinetisI2cTransfer *i2c;
    size_t spi_length;
    size_t spi_index;
    size_t i2c_length;
    size_t i2c_index;
    bool i2c_prefix;
    bool i2c_arbitration_loss;
} peripheral_expectations;

static peripheral_expectations expected;

static void expect_spi(const uint16_t *transfers, size_t length) {
    expected.spi = transfers;
    expected.spi_length = length;
    expected.spi_index = 0;
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
        fprintf(stderr, "unexpected SPI transfer %zu: 0x%04x\n", expected.spi_index, transfer.data);
        return false;
    }
    ++expected.spi_index;
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
        fprintf(stderr, "firmware stopped: reason %u, fault 0x%08x, PC 0x%08x\n",
                (unsigned)result.stop, fault_status, cortex_m4_get_register(cpu, 15));
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

static bool hardware_configuration_valid(Kinetis *device) {
    uint32_t power_mode = 0;
    uint32_t regulator = 0;
    uint32_t i2c_filter = 0;
    uint32_t clock = kinetis_core_clock_hz(device);

    bool power_mode_read = kinetis_read(device, SMC_PMSTAT_ADDRESS, &power_mode, 1);
    bool regulator_read = kinetis_read(device, PMC_REGSC_ADDRESS, &regulator, 1);
    bool i2c_filter_read = kinetis_read(device, I2C0_FLT_ADDRESS, &i2c_filter, 1);
    if (!power_mode_read || !regulator_read || !i2c_filter_read) {
        fprintf(stderr, "hardware register read failed: PMSTAT %u, REGSC %u, FLT %u, PC 0x%08x\n",
                power_mode_read, regulator_read, i2c_filter_read,
                cortex_m4_get_register(kinetis_cpu(device), 15));
        return false;
    }
    if (clock == EXPECTED_CORE_CLOCK_HZ && power_mode == 0x80 && regulator == 0x04 &&
        i2c_filter == 0x2a) {
        return true;
    }
    fprintf(stderr,
            "invalid hardware configuration: clock %u, PMSTAT 0x%02x, REGSC 0x%02x, "
            "FLT 0x%02x\n",
            clock, (unsigned)power_mode, (unsigned)regulator, (unsigned)i2c_filter);
    return false;
}

static bool spi_dma_is_byte_wide(Kinetis *device) {
    uint32_t transmit = DMA_TCD_BASE + DMA_SPI_TRANSMIT_CHANNEL * DMA_TCD_STRIDE;
    uint32_t receive = DMA_TCD_BASE + DMA_SPI_RECEIVE_CHANNEL * DMA_TCD_STRIDE;

    return register_equals(device, transmit + DMA_TCD_ATTR_OFFSET, 2, 0) &&
           register_equals(device, transmit + DMA_TCD_NBYTES_OFFSET, 4, 1) &&
           register_equals(device, transmit + DMA_TCD_DOFF_OFFSET, 2, 0) &&
           register_equals(device, receive + DMA_TCD_ATTR_OFFSET, 2, 0) &&
           register_equals(device, receive + DMA_TCD_NBYTES_OFFSET, 4, 1) &&
           register_equals(device, receive + DMA_TCD_DOFF_OFFSET, 2, 1);
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

static bool send_request(Kinetis *device, const uint8_t frame[WQR_FRAME_SIZE]) {
    uint8_t window[RECEIVE_WINDOW_SIZE] = {0};

    memcpy(window + RECEIVE_PREFIX_SIZE, frame, WQR_FRAME_SIZE);
    for (size_t index = 0; index < sizeof(window); ++index) {
        while (!kinetis_uart1_receive(device, window[index], 0)) {
            if (!step_firmware(device)) {
                return false;
            }
        }
    }
    return true;
}

static bool receive_response(Kinetis *device, uint8_t window[TRANSMIT_WINDOW_SIZE]) {
    size_t length = 0;

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

static bool exchange_frame(Kinetis *device, uint8_t type, uint8_t sequence, const uint8_t *payload,
                           size_t payload_length, uint8_t response[TRANSMIT_WINDOW_SIZE]) {
    uint8_t request[WQR_FRAME_SIZE];

    return wqr_protocol_build_frame(request, type, sequence, payload, payload_length) &&
           send_request(device, request) && receive_response(device, response);
}

static bool frame_integrity_valid(const uint8_t frame[WQR_FRAME_SIZE]) {
    uint16_t crc = (uint16_t)frame[61] | (uint16_t)((uint16_t)frame[62] << 8);

    return frame[0] == 0x7b && frame[63] == 0x7d &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) == crc;
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
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *payload = command_marker == 0 ? NULL : &command_marker;
    size_t payload_length = command_marker == 0 ? 0 : 1;

    return exchange_frame(device, WQR_PAYLOAD_STATUS, request_sequence, payload, payload_length,
                          response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), command_marker);
}

static bool exchange_measured_status(Kinetis *device, uint8_t request_sequence) {
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;
    uint16_t sensor_value = (uint16_t)wqr_sensor_value(SENSOR_SAMPLE);

    return exchange_frame(device, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0, response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), 0) &&
           frame[5] == INPUT_FLAGS && frame[6] == (uint8_t)sensor_value &&
           frame[7] == (uint8_t)(sensor_value >> 8);
}

static bool recover_from_noise(Kinetis *device) {
    uint8_t noise[WQR_FRAME_SIZE] = {0};
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    return send_request(device, noise) && receive_response(device, response) &&
           nack_response_valid(response);
}

static bool recover_from_bad_end_marker(Kinetis *device) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    if (!wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, 1, NULL, 0)) {
        return false;
    }
    request[WQR_FRAME_SIZE - 1] = 0;
    return send_request(device, request) && receive_response(device, response) &&
           nack_response_valid(response);
}

static bool reject_invalid_crc(Kinetis *device, uint8_t request_sequence) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    if (!wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0)) {
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

    if (!wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0)) {
        return false;
    }
    request[3] = WQR_FRAME_PAYLOAD_SIZE + 1;
    crc = wqr_protocol_crc(request + 1, WQR_FRAME_BODY_SIZE);
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
        !frame_integrity_valid(frame) || frame[1] != 1 || frame[4] != WQR_PAYLOAD_I2C) {
        return false;
    }
    return exchange_frame(device, 0x40 | WQR_PAYLOAD_STATUS, (uint8_t)(request_sequence + 1), NULL,
                          0, response) &&
           nack_response_valid(response);
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
    bool transfer_control;

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

static bool exchange_after_reconnect(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;
    bool transfer_control;

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(0xa0 + index);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 0;
    expect_spi(NULL, 0);
    if (!kinetis_gpio_drive(device, GPIO_PORT_C, 2, true) ||
        !exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload, sizeof(payload),
                        response) ||
        !frame_integrity_valid(frame) || frame[1] != WQR_PAYLOAD_PRIMARY_SPI ||
        frame[2] != (uint8_t)(request_sequence + 1) || (frame[60] & 2) != 0 ||
        !spi_expectations_met() || !kinetis_gpio_pin(device, GPIO_PORT_C, 3, &transfer_control) ||
        transfer_control) {
        return false;
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    if (!run_firmware(device, IDLE_INSTRUCTIONS) ||
        !kinetis_gpio_drive(device, GPIO_PORT_C, 2, false) ||
        !exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, (uint8_t)(request_sequence + 1), payload,
                        sizeof(payload), response) ||
        !primary_response_valid(response, (uint8_t)(request_sequence + 2), payload, false) ||
        !spi_expectations_met()) {
        return false;
    }
    if (!run_firmware(device, IDLE_INSTRUCTIONS) ||
        !queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE)) {
        return false;
    }
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    return exchange_frame(device, WQR_PAYLOAD_PRIMARY_SPI, (uint8_t)(request_sequence + 2), payload,
                          sizeof(payload), response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 3), payload, true) &&
           spi_expectations_met();
}

static bool exchange_alternate_spi(Kinetis *device, uint8_t request_sequence) {
    const uint16_t first_transmit[] = {0};
    const uint16_t second_transmit[] = {UINT16_C(0x1234)};
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0x34, 0x12};
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(first_transmit, 1);
    if (!kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, 0, 0) ||
        !exchange_frame(device, WQR_PAYLOAD_ALTERNATE_SPI, request_sequence, payload,
                        sizeof(payload), response) ||
        !frame_integrity_valid(frame) || frame[1] != WQR_PAYLOAD_ALTERNATE_SPI ||
        frame[2] != (uint8_t)(request_sequence + 1) || frame[3] != WQR_FRAME_PAYLOAD_SIZE ||
        frame[4] != 0 || frame[5] != 0 || (frame[60] & 2) == 0 || !spi_expectations_met()) {
        return false;
    }
    if (!run_firmware(device, IDLE_INSTRUCTIONS) ||
        !kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, UINT16_C(0x1234), 0)) {
        return false;
    }
    expect_spi(second_transmit, 1);
    return exchange_frame(device, WQR_PAYLOAD_ALTERNATE_SPI, (uint8_t)(request_sequence + 1),
                          payload, sizeof(payload), response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_ALTERNATE_SPI &&
           frame[2] == (uint8_t)(request_sequence + 2) && frame[3] == WQR_FRAME_PAYLOAD_SIZE &&
           frame[4] == payload[0] && frame[5] == payload[1] && (frame[60] & 2) != 0 &&
           spi_expectations_met();
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
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c(NULL, 0);
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

static bool configure_inputs(Kinetis *device) {
    return kinetis_set_adc_input(device, 0, KINETIS_ADC_MUX_A, 23, SENSOR_SAMPLE) &&
           kinetis_gpio_drive(device, GPIO_PORT_A, 4, false) &&
           kinetis_gpio_drive(device, GPIO_PORT_A, 18, true) &&
           kinetis_gpio_drive(device, GPIO_PORT_A, 19, false);
}

static bool software_reset_recorded(Kinetis *device) {
    uint8_t reset_status;

    return kinetis_read(device, UINT32_C(0x4007f001), &reset_status, sizeof(reset_status)) &&
           (reset_status & 4) != 0;
}

static bool firmware_passes(const char *path, KinetisPackage package) {
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
    coverage = cortex_m4_coverage_create(APPLICATION_BASE, FLASH_SIZE - APPLICATION_BASE);
    if (coverage == NULL) {
        kinetis_destroy(device);
        return false;
    }
    cortex_m4_set_coverage(kinetis_cpu(device), coverage);
    VERIFY_STAGE("load firmware", configure_inputs(device) && load_firmware(device, path));
    VERIFY_STAGE("start firmware", kinetis_reset(device) &&
                                       kinetis_set_reset_state(device, 1, true) &&
                                       run_firmware(device, STARTUP_INSTRUCTIONS));
    VERIFY_STAGE("hardware configuration", hardware_configuration_valid(device));
    VERIFY_STAGE("status", exchange_status(device, 0, 0));
    VERIFY_STAGE("UART response guard",
                 run_firmware(device, INTERRUPT_INSTRUCTIONS) && uart_guard_is_exact(device));
    VERIFY_STAGE("measured status", run_firmware(device, SENSOR_SETTLE_INSTRUCTIONS) &&
                                        exchange_measured_status(device, 1));
    VERIFY_STAGE("UART noise recovery",
                 run_firmware(device, IDLE_INSTRUCTIONS) && recover_from_noise(device));
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
    VERIFY_STAGE("primary SPI", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                    exchange_primary_spi(device, 3) &&
                                    spi_dma_is_byte_wide(device) && spi_format_is(device, 8, true));
    VERIFY_STAGE("repeated primary SPI", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                             exchange_repeated_primary_spi(device, 5));
    VERIFY_STAGE("SPI reconnect",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_after_reconnect(device, 6) &&
                     spi_dma_is_byte_wide(device) && spi_format_is(device, 8, true));
    VERIFY_STAGE("alternate SPI", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                      exchange_alternate_spi(device, 9) &&
                                      spi_format_is(device, 16, false));
    VERIFY_STAGE("I2C write",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_write(device, 11));
    VERIFY_STAGE("I2C NACK",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_nack(device, 12));
    VERIFY_STAGE("I2C arbitration loss", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                             exchange_i2c_arbitration_loss(device, 13));
    VERIFY_STAGE("empty I2C read",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_empty_i2c_read(device, 14));
    VERIFY_STAGE("I2C read",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_i2c_read(device, 15));
    VERIFY_STAGE("fragmented I2C read", run_firmware(device, IDLE_INSTRUCTIONS) &&
                                            exchange_fragmented_i2c_read(device, 16));
    VERIFY_STAGE("chunked I2C response",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_chunked_i2c_read(device, 18));
    VERIFY_STAGE("software reset request",
                 run_firmware(device, IDLE_INSTRUCTIONS) && exchange_status(device, 21, 0xaa) &&
                     run_firmware(device, STARTUP_INSTRUCTIONS) && software_reset_recorded(device));
    VERIFY_STAGE("status after reset",
                 exchange_status(device, 0, 0) && hardware_configuration_valid(device));
    passed = true;

finished:
    coverage_result = cortex_m4_coverage_result(coverage);
    uninitialized_reads = kinetis_get_uninitialized_sram_read_count(device);
    passed = passed && coverage_result.outside_range == 0 && uninitialized_reads == 0;
    printf("%s: %zu unique, %llu executed, %zu branches, %llu invalid reads\n", path,
           coverage_result.unique_instructions, (unsigned long long)coverage_result.instructions,
           coverage_result.observed_branch_sites, (unsigned long long)uninitialized_reads);
    cortex_m4_set_coverage(kinetis_cpu(device), NULL);
    cortex_m4_coverage_destroy(coverage);
    kinetis_destroy(device);
    return passed;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return firmware_passes(argv[1], KINETIS_PACKAGE_LH_64_LQFP) ? EXIT_SUCCESS : EXIT_FAILURE;
}
