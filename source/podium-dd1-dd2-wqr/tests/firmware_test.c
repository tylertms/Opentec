#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cortex_m4_firmware_image.h"
#include "kinetis_k22.h"
#include "protocol.h"

enum {
    RECEIVE_PREFIX_SIZE = 4,
    RECEIVE_WINDOW_SIZE = RECEIVE_PREFIX_SIZE + WQR_FRAME_SIZE,
    TRANSMIT_WINDOW_SIZE = WQR_FRAME_SIZE + 8,
    STARTUP_INSTRUCTIONS = 50000,
    RESPONSE_INSTRUCTION_LIMIT = 1000000,
    GPIO_PORT_C = 2,
    APPLICATION_BASE = 0xa000
};

static bool service_spi(KinetisK22 *device) {
    KinetisK22SpiTransfer transfer;

    return !kinetis_k22_spi_transfer(device, KINETIS_K22_SERIAL_SPI0, &transfer) ||
           kinetis_k22_serial_receive(device, KINETIS_K22_SERIAL_SPI0, transfer.data, 0);
}

static bool service_i2c(KinetisK22 *device) {
    KinetisK22I2cTransfer transfer;

    if (!kinetis_k22_i2c_transfer(device, KINETIS_K22_SERIAL_I2C0, &transfer)) {
        return true;
    }
    if (transfer.type == KINETIS_K22_I2C_WRITE) {
        return kinetis_k22_i2c_acknowledge(device, KINETIS_K22_SERIAL_I2C0, true);
    }
    if (transfer.type == KINETIS_K22_I2C_READ) {
        return kinetis_k22_i2c_receive(device, KINETIS_K22_SERIAL_I2C0, 0x5a);
    }
    return true;
}

static bool step_firmware(KinetisK22 *device) {
    CortexM4 *cpu = kinetis_k22_cpu(device);
    CortexM4Result result = cortex_m4_step(cpu);

    if (result.stop == CORTEX_M4_STOP_LOCKUP || result.stop == CORTEX_M4_STOP_UNSUPPORTED ||
        result.stop == CORTEX_M4_STOP_BUS_FAULT || result.stop == CORTEX_M4_STOP_USAGE_FAULT ||
        cortex_m4_get_fault_status(cpu) != 0) {
        return false;
    }
    return service_spi(device) && service_i2c(device);
}

static bool run_firmware(KinetisK22 *device, size_t instructions) {
    while (instructions-- != 0) {
        if (!step_firmware(device)) {
            return false;
        }
    }
    return true;
}

static bool load_firmware(KinetisK22 *device, const char *path) {
    return cortex_m4_load_elf(device, path, NULL) ||
           cortex_m4_load_binary(device, path, APPLICATION_BASE);
}

static bool send_request(KinetisK22 *device, const uint8_t frame[WQR_FRAME_SIZE]) {
    uint8_t window[RECEIVE_WINDOW_SIZE] = {0};

    memcpy(window + RECEIVE_PREFIX_SIZE, frame, WQR_FRAME_SIZE);
    for (size_t index = 0; index < sizeof(window); ++index) {
        while (!kinetis_k22_uart1_receive(device, window[index], 0)) {
            if (!step_firmware(device)) {
                return false;
            }
        }
    }
    return true;
}

static bool receive_response(KinetisK22 *device, uint8_t window[TRANSMIT_WINDOW_SIZE]) {
    size_t length = 0;

    for (size_t instruction = 0; instruction < RESPONSE_INSTRUCTION_LIMIT; ++instruction) {
        uint8_t value;

        while (length < TRANSMIT_WINDOW_SIZE && kinetis_k22_uart1_transmit(device, &value)) {
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

static bool status_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE], uint8_t sequence,
                                  uint8_t command_marker) {
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;
    uint16_t crc = (uint16_t)frame[61] | (uint16_t)((uint16_t)frame[62] << 8);

    return frame[0] == 0x7b && frame[1] == WQR_PAYLOAD_STATUS && frame[2] == sequence &&
           frame[3] == WQR_STATUS_SIZE && frame[4] == 7 && frame[63] == 0x7d &&
           frame[18] == command_marker && wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) == crc;
}

static bool nack_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE]) {
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;
    uint16_t crc = (uint16_t)frame[61] | (uint16_t)((uint16_t)frame[62] << 8);

    return frame[0] == 0x7b && frame[1] == 0 && frame[63] == 0x7d &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) == crc;
}

static bool exchange_status(KinetisK22 *device, uint8_t request_sequence, uint8_t command_marker) {
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *payload = command_marker == 0 ? NULL : &command_marker;
    size_t payload_length = command_marker == 0 ? 0 : 1;

    return wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, request_sequence, payload,
                                    payload_length) &&
           send_request(device, request) && receive_response(device, response) &&
           status_response_valid(response, (uint8_t)(request_sequence + 1), command_marker);
}

static bool recover_from_noise(KinetisK22 *device) {
    uint8_t noise[WQR_FRAME_SIZE] = {0};
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    return send_request(device, noise) && receive_response(device, response) &&
           nack_response_valid(response);
}

static bool primary_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE], uint8_t sequence,
                                   const uint8_t payload[WQR_FRAME_PAYLOAD_SIZE], bool exchanged) {
    static const uint8_t empty_response[WQR_SPI_TRANSFER_SIZE];
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;
    const uint8_t *expected = exchanged ? payload : empty_response;
    uint16_t crc = (uint16_t)frame[61] | (uint16_t)((uint16_t)frame[62] << 8);

    return frame[0] == 0x7b && frame[1] == WQR_PAYLOAD_PRIMARY_SPI && frame[2] == sequence &&
           frame[3] == WQR_FRAME_PAYLOAD_SIZE && frame[63] == 0x7d && (frame[60] & 2) != 0 &&
           memcmp(frame + 4, expected, WQR_SPI_TRANSFER_SIZE) == 0 &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) == crc;
}

static bool exchange_primary_spi(KinetisK22 *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];

    for (size_t index = 0; index < WQR_SPI_TRANSFER_SIZE; ++index) {
        payload[index] = (uint8_t)(index + 1);
    }
    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    return kinetis_k22_gpio_drive(device, GPIO_PORT_C, 2, false) &&
           wqr_protocol_build_frame(request, WQR_PAYLOAD_PRIMARY_SPI, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 1), payload, false) &&
           run_firmware(device, STARTUP_INSTRUCTIONS) &&
           wqr_protocol_build_frame(request, WQR_PAYLOAD_PRIMARY_SPI,
                                    (uint8_t)(request_sequence + 1), payload, sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           primary_response_valid(response, (uint8_t)(request_sequence + 2), payload, true);
}

static bool exchange_alternate_spi(KinetisK22 *device, uint8_t request_sequence) {
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0x34, 0x12};
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    payload[WQR_FRAME_PAYLOAD_SIZE - 1] = 1;
    return wqr_protocol_build_frame(request, WQR_PAYLOAD_ALTERNATE_SPI, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           frame[0] == 0x7b && frame[1] == WQR_PAYLOAD_ALTERNATE_SPI &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == WQR_FRAME_PAYLOAD_SIZE &&
           frame[4] == payload[0] && frame[5] == payload[1] && (frame[60] & 2) != 0 &&
           frame[63] == 0x7d &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) ==
               (uint16_t)(frame[61] | (uint16_t)(frame[62] << 8));
}

static bool exchange_i2c_write(KinetisK22 *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa0, 0x10, 0x22};
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    return wqr_protocol_build_frame(request, WQR_PAYLOAD_I2C, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           frame[0] == 0x7b && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 3 && frame[4] == 1 &&
           frame[5] == payload[1] && frame[6] == payload[2] && frame[63] == 0x7d &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) ==
               (uint16_t)(frame[61] | (uint16_t)(frame[62] << 8));
}

static bool exchange_i2c_read(KinetisK22 *device, uint8_t request_sequence) {
    const uint8_t payload[] = {0, 0xa1, 0x10, 3, 0};
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    const uint8_t *frame = response + RECEIVE_PREFIX_SIZE;

    return wqr_protocol_build_frame(request, WQR_PAYLOAD_I2C, request_sequence, payload,
                                    sizeof(payload)) &&
           send_request(device, request) && receive_response(device, response) &&
           frame[0] == 0x7b && frame[1] == WQR_PAYLOAD_I2C &&
           frame[2] == (uint8_t)(request_sequence + 1) && frame[3] == 5 && frame[4] == 1 &&
           frame[5] == payload[1] && frame[6] == 0x5a && frame[7] == 0x5a && frame[8] == 0x5a &&
           frame[63] == 0x7d &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) ==
               (uint16_t)(frame[61] | (uint16_t)(frame[62] << 8));
}

int main(int argc, char **argv) {
    KinetisK22Configuration configuration =
        kinetis_k22_configuration(KINETIS_K22_PROFILE_MK22F12810);
    KinetisK22 *device;
    bool passed = false;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    configuration.vector_table_address = APPLICATION_BASE;
    device = kinetis_k22_create(configuration);
    if (device == NULL) {
        return EXIT_FAILURE;
    }
    if (load_firmware(device, argv[1]) && kinetis_k22_reset(device) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_status(device, 0, 0) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && recover_from_noise(device) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_primary_spi(device, 1) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_alternate_spi(device, 3) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_i2c_write(device, 4) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_i2c_read(device, 5) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_status(device, 6, 0xaa) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) && exchange_status(device, 0, 0)) {
        passed = true;
    }
    kinetis_k22_destroy(device);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
