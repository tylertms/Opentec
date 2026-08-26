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
    RESPONSE_INSTRUCTION_LIMIT = 1000000
};

static bool step_firmware(KinetisK22 *device) {
    CortexM4 *cpu = kinetis_k22_cpu(device);
    CortexM4Result result = cortex_m4_step(cpu);

    return result.stop != CORTEX_M4_STOP_LOCKUP && result.stop != CORTEX_M4_STOP_UNSUPPORTED &&
           result.stop != CORTEX_M4_STOP_BUS_FAULT && result.stop != CORTEX_M4_STOP_USAGE_FAULT &&
           cortex_m4_get_fault_status(cpu) == 0;
}

static bool run_firmware(KinetisK22 *device, size_t instructions) {
    while (instructions-- != 0) {
        if (!step_firmware(device)) {
            return false;
        }
    }
    return true;
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

static bool status_response_valid(const uint8_t window[TRANSMIT_WINDOW_SIZE]) {
    const uint8_t *frame = window + RECEIVE_PREFIX_SIZE;
    uint16_t crc = (uint16_t)frame[61] | (uint16_t)((uint16_t)frame[62] << 8);

    return frame[0] == 0x7b && frame[1] == WQR_PAYLOAD_STATUS && frame[2] == 1 &&
           frame[3] == WQR_STATUS_SIZE && frame[4] == 7 && frame[63] == 0x7d &&
           wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE) == crc;
}

int main(int argc, char **argv) {
    KinetisK22Configuration configuration =
        kinetis_k22_configuration(KINETIS_K22_PROFILE_MK22F12810);
    KinetisK22 *device;
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[TRANSMIT_WINDOW_SIZE];
    bool passed = false;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    configuration.vector_table_address = 0xa000;
    device = kinetis_k22_create(configuration);
    if (device == NULL) {
        return EXIT_FAILURE;
    }
    if (cortex_m4_load_elf(device, argv[1], NULL) && kinetis_k22_reset(device) &&
        run_firmware(device, STARTUP_INSTRUCTIONS) &&
        wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, 0, NULL, 0) &&
        send_request(device, request) && receive_response(device, response) &&
        status_response_valid(response) && run_firmware(device, STARTUP_INSTRUCTIONS)) {
        passed = true;
    }
    kinetis_k22_destroy(device);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
