#include <assert.h>
#include <string.h>

#include "protocol.h"

typedef struct {
    unsigned int resets;
    unsigned int spi_transfers;
    uint8_t i2c_address;
    uint8_t i2c_command;
    bool transfer_ready;
    bool transfer_control;
} test_io;

static bool test_spi_transfer(void *context, const uint8_t *transmit, uint8_t *receive,
                              size_t length) {
    test_io *io = context;

    ++io->spi_transfers;
    memcpy(receive, transmit, length);
    return true;
}

static bool test_transfer_ready(void *context) {
    test_io *io = context;

    return io->transfer_ready;
}

static void test_set_transfer_control(void *context, bool asserted) {
    test_io *io = context;

    io->transfer_control = asserted;
}

static bool test_i2c_read(void *context, uint8_t address, uint8_t command, uint8_t *data,
                          size_t length) {
    test_io *io = context;
    size_t index;

    io->i2c_address = address;
    io->i2c_command = command;
    for (index = 0; index < length; ++index) {
        data[index] = (uint8_t)(command + index);
    }
    return true;
}

static void test_reset(void *context) {
    test_io *io = context;

    ++io->resets;
}

static void test_crc(void) {
    static const uint8_t value[] = "123456789";

    assert(wqr_protocol_crc(value, sizeof(value) - 1) == 0x2189);
}

static void test_status_and_reset(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .request_reset = test_reset};
    wqr_protocol protocol;
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[WQR_FRAME_SIZE];
    const uint8_t command = 0xaa;

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(request, WQR_PAYLOAD_STATUS, 7, &command, 1));
    assert(wqr_protocol_receive(&protocol, request));
    assert(!wqr_protocol_response(&protocol, response));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, response));
    assert(response[0] == 0x7b);
    assert(response[1] == WQR_PAYLOAD_STATUS);
    assert(response[2] == 8);
    assert(response[3] == WQR_STATUS_SIZE);
    assert(response[4] == 7);
    assert(response[18] == 0xaa);
    assert(wqr_protocol_crc(response + 1, WQR_FRAME_BODY_SIZE) ==
           (uint16_t)(response[61] | (uint16_t)(response[62] << 8)));
    wqr_protocol_response_sent(&protocol);
    assert(state.resets == 1);
}

static void test_fragmented_i2c_read(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .i2c_read = test_i2c_read};
    wqr_protocol protocol;
    uint8_t request[WQR_FRAME_SIZE];
    uint8_t response[WQR_FRAME_SIZE];
    uint8_t first[WQR_FRAME_PAYLOAD_SIZE] = {0, 0xa1, 0x10, 3, 0};
    const uint8_t last[] = {0};

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(request, 0x10 | WQR_PAYLOAD_I2C, 0, first, sizeof(first)));
    assert(wqr_protocol_receive(&protocol, request));
    assert(wqr_protocol_response(&protocol, response));
    assert((response[1] & 0x0f) == 1);
    assert(wqr_protocol_build_frame(request, 0x40 | WQR_PAYLOAD_I2C, 1, last, sizeof(last)));
    assert(wqr_protocol_receive(&protocol, request));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, response));
    assert(state.i2c_address == 0xa0);
    assert(state.i2c_command == 0x10);
    assert(response[4] == 1);
}

static void test_invalid_frame(void) {
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE] = {0};
    uint8_t response[WQR_FRAME_SIZE];

    wqr_protocol_init(&protocol, NULL);
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(protocol.error_count == 1);
    assert(wqr_protocol_response(&protocol, response));
    assert(response[1] == 0);
    assert(response[4] == 0xff);
}

static void test_primary_spi_handshake(void) {
    test_io state = {.transfer_ready = true};
    wqr_io io = {
        .context = &state,
        .spi_transfer = test_spi_transfer,
        .transfer_ready = test_transfer_ready,
        .set_transfer_control = test_set_transfer_control,
    };
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};

    payload[0] = 0x5a;
    payload[56] = 1;
    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 0, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(state.transfer_control);
    assert(state.spi_transfers == 0);
    assert((frame[60] & 2) != 0);
    assert(protocol.transfer_state == WQR_TRANSFER_WAITING);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 1, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(state.spi_transfers == 0);
    assert(protocol.transfer_state == WQR_TRANSFER_DETECTED);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 2, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(state.spi_transfers == 1);
    assert(frame[4] == 0x5a);
    assert(protocol.transfer_state == WQR_TRANSFER_READY);
    assert(protocol.transfer_detail == 0);
}

static void test_transfer_not_ready(void) {
    test_io state = {0};
    wqr_io io = {
        .context = &state,
        .spi_transfer = test_spi_transfer,
        .transfer_ready = test_transfer_ready,
        .set_transfer_control = test_set_transfer_control,
    };
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};

    payload[56] = 1;
    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 0, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(state.transfer_control);
    assert(state.spi_transfers == 0);
    assert((frame[60] & 2) == 0);
    assert(protocol.transfer_state == WQR_TRANSFER_WAITING);
}

static void test_chunked_response(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .i2c_read = test_i2c_read};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t request[] = {0, 0xa1, 0x20, 60, 0};

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 11, request, sizeof(request)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[1] == (0x10 | WQR_PAYLOAD_I2C));
    assert(frame[2] == 12);
    assert(frame[3] == WQR_FRAME_PAYLOAD_SIZE);

    assert(wqr_protocol_build_frame(frame, 1, 12, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[1] == (0x40 | WQR_PAYLOAD_I2C));
    assert(frame[2] == 13);
    assert(frame[3] == 5);
}

static void test_sensor(void) {
    assert(wqr_sensor_value(0) == 999);
    assert(wqr_sensor_value(4095) > 100);
}

int main(void) {
    test_crc();
    test_status_and_reset();
    test_fragmented_i2c_read();
    test_invalid_frame();
    test_primary_spi_handshake();
    test_transfer_not_ready();
    test_chunked_response();
    test_sensor();
    return 0;
}
