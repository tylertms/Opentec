#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cortex_m4_firmware_image.h"
#include "kinetis.h"
#include "protocol.h"

enum {
    RECEIVE_PREFIX_SIZE = 4,
    RECEIVE_WINDOW_SIZE = RECEIVE_PREFIX_SIZE + WQR_FRAME_SIZE,
    TRANSMIT_WINDOW_SIZE = WQR_FRAME_SIZE + 8,
    STARTUP_INSTRUCTIONS = 50000,
    RESPONSE_INSTRUCTION_LIMIT = 1000000,
    SENSOR_SETTLE_INSTRUCTIONS = 10000000,
    GPIO_PORT_A = 0,
    GPIO_PORT_C = 2,
    SENSOR_SAMPLE = 2048,
    INPUT_FLAGS = 5,
    APPLICATION_BASE = 0xa000,
    SRAM_SIZE = 0x4000,
    FLASH_SIZE = 0x20000
};

typedef struct {
    const uint16_t *spi;
    const KinetisI2cTransfer *i2c;
    size_t spi_length;
    size_t spi_index;
    size_t i2c_length;
    size_t i2c_index;
    bool i2c_prefix;
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

    if (result.stop == CORTEX_M4_STOP_LOCKUP || result.stop == CORTEX_M4_STOP_UNSUPPORTED ||
        result.stop == CORTEX_M4_STOP_BUS_FAULT || result.stop == CORTEX_M4_STOP_USAGE_FAULT ||
        cortex_m4_get_fault_status(cpu) != 0) {
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
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *payload = command_marker == 0 ? NULL : &command_marker;
    size_t payload_length = command_marker == 0 ? 0 : 1;

    return wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, request_sequence, payload,
                                    payload_length) &&
           send_request(device, request) && receive_response(device, response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), command_marker);
}

static bool exchange_measured_status(Kinetis *device, uint8_t request_sequence) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;
    uint16_t sensor_value = (uint16_t)wqr_sensor_value(SENSOR_SAMPLE);

    return wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, request_sequence, NULL, 0) &&
           send_request(device, request) && receive_response(device, response) &&
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

static bool primary_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE], uint8_t sequence,
                                   const uint8_t payload[WQR_FRAME_PAYLOAD_SIZE], bool exchanged) {
    static const uint8_t empty_response[WQR_SPI_TRANSFER_SIZE];
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;
    const uint8_t *expected = exchanged ? payload : empty_response;

    return frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_PRIMARY_SPI &&
           frame[2] == sequence && frame[3] == WQR_FRAME_PAYLOAD_SIZE && (frame[60] & 2) != 0 &&
           memcmp(frame + 4, expected, WQR_SPI_TRANSFER_SIZE) == 0;
}

static bool exchange_primary_spi(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    bool transfer_control;

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(index + 1);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(NULL, 0);
    if (!kinetis_gpio_drive(device, GPIO_PORT_C, 2, false) ||
        !wqr_protocol_build_frame(request, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload,
                                  sizeof(payload)) ||
        !send_request(device, request) || !receive_response(device, response) ||
        !primary_response_valid(response, (uint8_t)(request_sequence + 1), payload, false) ||
        !spi_expectations_met() || !kinetis_gpio_pin(device, GPIO_PORT_C, 3, &transfer_control) ||
        !transfer_control || !run_firmware(device, STARTUP_INSTRUCTIONS) ||
        !queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE)) {
        return false;
    }
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    return wqr_protocol_build_frame(request, WQR_PAYLOAD_PRIMARY_SPI,
                                    (uint8_t)(request_sequence + 1), payload, sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 2), payload, true) &&
           spi_expectations_met();
}

static bool exchange_repeated_primary_spi(Kinetis *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t transmitted[WQR_SPI_TRANSFER_SIZE];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(WQR_SPI_TRANSFER_SIZE - index);
        transmitted[index] = payload[index];
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(transmitted, WQR_SPI_TRANSFER_SIZE);
    return queue_spi_response(device, payload, WQR_SPI_TRANSFER_SIZE) &&
           wqr_protocol_build_frame(request, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 1), payload, true) &&
           spi_expectations_met();
}

static bool exchange_alternate_spi(Kinetis *device, uint8_t request_sequence) {
    const uint16_t first_transmit[] = {0};
    const uint16_t second_transmit[] = {UINT16_C(0x1234)};
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0x34, 0x12};
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    expect_spi(first_transmit, 1);
    if (!kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, 0, 0) ||
        !wqr_protocol_build_frame(request, WQR_PAYLOAD_ALTERNATE_SPI, request_sequence, payload,
                                  sizeof(payload)) ||
        !send_request(device, request) || !receive_response(device, response) ||
        !frame_integrity_valid(frame) || frame[1] != WQR_PAYLOAD_ALTERNATE_SPI ||
        frame[2] != (uint8_t)(request_sequence + 1) || frame[3] != WQR_FRAME_PAYLOAD_SIZE ||
        frame[4] != 0 || frame[5] != 0 || (frame[60] & 2) == 0 || !spi_expectations_met()) {
        return false;
    }
    if (!run_firmware(device, STARTUP_INSTRUCTIONS) ||
        !kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, UINT16_C(0x1234), 0)) {
        return false;
    }
    expect_spi(second_transmit, 1);
    return wqr_protocol_build_frame(request, WQR_PAYLOAD_ALTERNATE_SPI,
                                    (uint8_t)(request_sequence + 1), payload, sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
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
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c(transfers, sizeof(transfers) / sizeof(transfers[0]));
    return wqr_protocol_build_frame(request, WQR_PAYLOAD_I2C, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 1 &&
           frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met();
}

static bool exchange_i2c_timeout(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa0, 0x10, 0x22};
    const KinetisI2cTransfer transfers[] = {
        {KINETIS_I2C_START, 0},
        {KINETIS_I2C_WRITE, 0xa0},
    };
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_prefix(transfers, sizeof(transfers) / sizeof(transfers[0]));
    bool valid = kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, false) &&
                 wqr_protocol_build_frame(request, WQR_PAYLOAD_I2C, request_sequence, payload,
                                          sizeof(payload)) &&
                 send_request(device, request) && receive_response(device, response) &&
                 frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
                 frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 0 &&
                 frame[5] == payload[1] && frame[6] == payload[2] && i2c_expectations_met();
    return kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, true) && valid;
}

static bool exchange_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x10, 3, 0};
    KinetisI2cTransfer transfers[9];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    expect_i2c_read(transfers, 0x10, 3);
    return wqr_protocol_build_frame(request, WQR_PAYLOAD_I2C, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 5 && frame[4] == 1 &&
           frame[5] == payload[1] && frame[6] == 0x5a && frame[7] == 0x5a && frame[8] == 0x5a &&
           i2c_expectations_met();
}

static bool exchange_fragmented_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t first_payload[] = {0, 0xa1, 0x10};
    const uint8_t last_payload[] = {3, 0};
    KinetisI2cTransfer transfers[9];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    if (!wqr_protocol_build_frame(request, 0x10 | WQR_PAYLOAD_I2C, request_sequence, first_payload,
                                  sizeof(first_payload)) ||
        !send_request(device, request) || !receive_response(device, response) ||
        !frame_integrity_valid(frame) || frame[1] != 1 || frame[4] != WQR_PAYLOAD_I2C) {
        return false;
    }
    expect_i2c_read(transfers, 0x10, 3);
    return run_firmware(device, STARTUP_INSTRUCTIONS) &&
           wqr_protocol_build_frame(request, 0x40 | WQR_PAYLOAD_I2C,
                                    (uint8_t)(request_sequence + 1), last_payload,
                                    sizeof(last_payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           frame_integrity_valid(frame) && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 2) && frame[3] == 5 && frame[4] == 1 &&
           frame[5] == 0xa1 && frame[6] == 0x5a && frame[7] == 0x5a && frame[8] == 0x5a &&
           i2c_expectations_met();
}

static bool exchange_chunked_i2c_read(Kinetis *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x20, 60, 0};
    KinetisI2cTransfer transfers[66];
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;
    expect_i2c_read(transfers, 0x20, 60);
    if (!wqr_protocol_build_frame(request, WQR_PAYLOAD_I2C, request_sequence, payload,
                                  sizeof(payload)) ||
        !send_request(device, request) || !receive_response(device, response) ||
        !frame_integrity_valid(frame) || frame[1] != (0x10 | WQR_PAYLOAD_I2C) ||
        frame[2] != (uint8_t)(request_sequence + 1) || frame[3] != WQR_FRAME_PAYLOAD_SIZE ||
        !i2c_expectations_met()) {
        return false;
    }
    return run_firmware(device, STARTUP_INSTRUCTIONS) &&
           wqr_protocol_build_frame(request, 1, (uint8_t)(request_sequence + 1), NULL, 0) &&
           send_request(device, request) && receive_response(device, response) &&
           frame_integrity_valid(frame) && frame[1] == (0x40 | WQR_PAYLOAD_I2C) &&
           frame[2] == (uint8_t)(request_sequence + 2) && frame[3] == 5 && frame[4] == 0x5a &&
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
    passed =
        configure_inputs(device) && load_firmware(device, path) && kinetis_reset(device) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_status(device, 0, 0) &&
        run_firmware(device, SENSOR_SETTLE_INSTRUCTIONS) && exchange_measured_status(device, 1) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && recover_from_noise(device) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && recover_from_bad_end_marker(device) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_primary_spi(device, 2) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_repeated_primary_spi(device, 4) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_alternate_spi(device, 5) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_i2c_write(device, 7) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_i2c_timeout(device, 8) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_i2c_read(device, 9) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_fragmented_i2c_read(device, 10) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_chunked_i2c_read(device, 12) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_status(device, 14, 0xaa) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && software_reset_recorded(device) &&
        exchange_status(device, 0, 0);
    coverage_result = cortex_m4_coverage_result(coverage);
    passed = passed && coverage_result.outside_range == 0;
    printf("%s: %zu unique instructions, %llu executed, %zu skipped, "
           "%zu/%zu observed branch outcomes across %zu sites, %zu fully covered, "
           "%llu branches executed (%llu taken, %llu not taken), %llu outside range\n",
           path, coverage_result.unique_instructions,
           (unsigned long long)coverage_result.instructions, coverage_result.unique_skipped,
           coverage_result.unique_branch_outcomes, coverage_result.unique_branch_sites * 2,
           coverage_result.unique_branch_sites, coverage_result.fully_covered_branch_sites,
           (unsigned long long)coverage_result.conditional_branches,
           (unsigned long long)coverage_result.branches_taken,
           (unsigned long long)coverage_result.branches_not_taken,
           (unsigned long long)coverage_result.outside_range);
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
