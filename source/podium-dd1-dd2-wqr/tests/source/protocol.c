#include "protocol.h"

#include <assert.h>
#include <string.h>

typedef struct {
    unsigned int resets;
    unsigned int i2c_reads;
    unsigned int i2c_writes;
    unsigned int spi_transfers;
    unsigned int spi_word_transfers;
    size_t i2c_length;
    uint8_t i2c_address;
    uint8_t i2c_command;
    uint8_t inputs;
    uint16_t spi_word;
    bool i2c_complete;
    bool i2c_started;
    bool spi_complete;
    bool spi_started;
    bool spi_word_complete;
    bool spi_word_started;
    bool transfer_ready;
    bool transfer_control;
} test_io;

static wqr_io_result test_spi_transfer(void *context, const uint8_t *transmit, uint8_t *receive,
                                       size_t length) {
    test_io *io = context;

    ++io->spi_transfers;
    memcpy(receive, transmit, length);
    return WQR_IO_SUCCEEDED;
}

static wqr_io_result test_pending_spi_transfer(void *context, const uint8_t *transmit,
                                               uint8_t *receive, size_t length) {
    test_io *io = context;

    if (!io->spi_started) {
        io->spi_started = true;
        ++io->spi_transfers;
    }
    if (!io->spi_complete) {
        return WQR_IO_PENDING;
    }
    memcpy(receive, transmit, length);
    return WQR_IO_SUCCEEDED;
}

static wqr_io_result test_pending_spi_word(void *context, uint16_t transmit, uint16_t *receive) {
    test_io *io = context;

    if (!io->spi_word_started) {
        io->spi_word_started = true;
        io->spi_word = transmit;
        ++io->spi_word_transfers;
    }
    if (!io->spi_word_complete) {
        return WQR_IO_PENDING;
    }
    io->spi_word_started = false;
    *receive = 0x1234;
    return WQR_IO_SUCCEEDED;
}

static bool test_transfer_ready(void *context) {
    test_io *io = context;

    return io->transfer_ready;
}

static void test_set_transfer_control(void *context, bool asserted) {
    test_io *io = context;

    io->transfer_control = asserted;
}

static wqr_io_result test_i2c_read(void *context, uint8_t address, uint8_t command, uint8_t *data,
                                   size_t length) {
    test_io *io = context;
    size_t index;

    ++io->i2c_reads;
    io->i2c_length = length;
    io->i2c_address = address;
    io->i2c_command = command;
    for (index = 0; index < length; ++index) {
        data[index] = (uint8_t)(command + index);
    }
    return WQR_IO_SUCCEEDED;
}

static wqr_io_result test_i2c_write(void *context, uint8_t address, const uint8_t *data,
                                    size_t length) {
    test_io *io = context;

    ++io->i2c_writes;
    io->i2c_address = address;
    io->i2c_command = length == 0 ? 0 : data[0];
    io->i2c_length = length;
    return WQR_IO_SUCCEEDED;
}

static wqr_io_result test_pending_i2c_read(void *context, uint8_t address, uint8_t command,
                                           uint8_t *data, size_t length) {
    test_io *io = context;
    size_t index;

    io->i2c_address = address;
    io->i2c_command = command;
    io->i2c_started = true;
    if (!io->i2c_complete) {
        return WQR_IO_PENDING;
    }
    for (index = 0; index < length; ++index) {
        data[index] = (uint8_t)(command + index);
    }
    return WQR_IO_SUCCEEDED;
}

static uint8_t test_read_inputs(void *context) {
    test_io *io = context;

    return io->inputs;
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

static void test_status_values(void) {
    test_io state = {.inputs = 0xff};
    wqr_io io = {.context = &state, .read_inputs = test_read_inputs, .request_reset = test_reset};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];

    wqr_protocol_init(&protocol, &io);
    wqr_protocol_set_sensor_sample(&protocol, 2048);
    for (size_t tick = 0; tick < 999; ++tick) {
        wqr_protocol_tick(&protocol);
    }
    assert(protocol.milliseconds == 999);
    assert(protocol.seconds == 0);
    wqr_protocol_tick(&protocol);
    assert(protocol.milliseconds == 0);
    assert(protocol.seconds == 1);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 0, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[5] == 7);
    assert(frame[6] == 64);
    assert(frame[7] == 0);
    assert(frame[8] == 1);
    assert(frame[16] == 1);
    wqr_protocol_response_sent(&protocol);
    assert(state.resets == 0);

    protocol.peer_ready_confirmed = true;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 1, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[16] == 2);

    protocol.transfer_enabled = true;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 2, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[16] == 4);
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

static void test_invalid_fragments(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .request_reset = test_reset};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t command = 0xaa;

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, 0x40 | WQR_PAYLOAD_STATUS, 200, &command, 1));
    assert(!wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    wqr_protocol_response_sent(&protocol);
    assert(state.resets == 0);
    assert(protocol.error_count == 1);

    assert(wqr_protocol_build_frame(frame, 0x10 | WQR_PAYLOAD_STATUS, 2, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_build_frame(frame, 0x40 | WQR_PAYLOAD_STATUS, 4, &command, 1));
    assert(!wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    wqr_protocol_response_sent(&protocol);
    assert(state.resets == 0);
    assert(protocol.error_count == 2);
}

static void test_invalid_frame(void) {
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t response[WQR_FRAME_SIZE];

    wqr_protocol_init(&protocol, NULL);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 0, NULL, 0));
    frame[0] = 0;
    assert(!wqr_protocol_receive(&protocol, frame));
    frame[0] = 0x7b;
    frame[63] = 0;
    assert(!wqr_protocol_receive(&protocol, frame));
    frame[63] = 0x7d;
    frame[61] ^= 1;
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(protocol.error_count == 3);
    assert(wqr_protocol_response(&protocol, response));
    assert(response[1] == 0);
    assert(response[4] == 0xff);
}

static void test_control_frames(void) {
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];

    wqr_protocol_init(&protocol, NULL);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 5, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));

    assert(wqr_protocol_build_frame(frame, 0, 5, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[1] == WQR_PAYLOAD_STATUS);

    assert(wqr_protocol_build_frame(frame, 1, 6, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(!wqr_protocol_response(&protocol, frame));

    assert(wqr_protocol_build_frame(frame, 0, 6, NULL, 0));
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[1] == 0);

    assert(wqr_protocol_build_frame(frame, 6, 7, NULL, 0));
    assert(!wqr_protocol_receive(&protocol, frame));
}

static void test_i2c_boundaries(void) {
    test_io state = {0};
    wqr_io io = {
        .context = &state,
        .i2c_write = test_i2c_write,
        .i2c_read = test_i2c_read,
    };
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t short_request[] = {0, 0xa0};
    const uint8_t write_request[] = {0, 0xa0, 0x33, 0x44};
    const uint8_t short_read[] = {0, 0xa1, 0x10, 0};
    const uint8_t oversized_read[] = {0, 0xa1, 0x10, 0xff, 0x01};
    const uint8_t empty_read[] = {0, 0xa1, 0x20, 0, 0};
    const uint8_t maximum_read[] = {0, 0xa1, 0x30, 0xfe, 0x01};

    wqr_protocol_init(&protocol, &io);
    assert(
        wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 0, short_request, sizeof(short_request)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[3] == 3);
    assert(frame[4] == 0);

    assert(
        wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 1, write_request, sizeof(write_request)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 1);
    assert(state.i2c_writes == 1);
    assert(state.i2c_address == 0xa0);
    assert(state.i2c_command == 0x33);
    assert(state.i2c_length == 2);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 2, short_read, sizeof(short_read)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 0);
    assert(state.i2c_reads == 0);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 3, oversized_read,
                                    sizeof(oversized_read)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 0);
    assert(state.i2c_reads == 0);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 4, empty_read, sizeof(empty_read)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[3] == 2);
    assert(frame[4] == 1);
    assert(state.i2c_reads == 1);
    assert(state.i2c_command == 0x20);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 5, maximum_read, sizeof(maximum_read)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(protocol.transmit_length == WQR_TRANSFER_CAPACITY);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[1] == (WQR_FRAME_FIRST | WQR_PAYLOAD_I2C));
    assert(frame[2] == 6);
    assert(frame[3] == WQR_FRAME_PAYLOAD_SIZE);
    assert(frame[4] == 1);
    assert(frame[5] == 0xa1);
    assert(frame[6] == 0x30);
    assert(state.i2c_reads == 2);
    assert(state.i2c_length == WQR_TRANSFER_CAPACITY - 2);

    wqr_protocol_init(&protocol, NULL);
    assert(
        wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 6, write_request, sizeof(write_request)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 0);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 7, empty_read, sizeof(empty_read)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 0);
}

static void test_rejected_input(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .i2c_write = test_i2c_write};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t response[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint8_t last_payload[WQR_FRAME_PAYLOAD_SIZE - 1] = {0};
    uint16_t crc;

    assert(!wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 0, payload,
                                     WQR_FRAME_PAYLOAD_SIZE + 1));
    assert(!wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 0, NULL, 1));

    wqr_protocol_init(&protocol, NULL);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 0, NULL, 0));
    frame[3] = WQR_FRAME_PAYLOAD_SIZE + 1;
    crc = wqr_protocol_crc(frame + 1, WQR_FRAME_BODY_SIZE);
    frame[61] = (uint8_t)crc;
    frame[62] = (uint8_t)(crc >> 8);
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(protocol.error_count == 1);

    assert(wqr_protocol_build_frame(frame, 0x30 | WQR_PAYLOAD_STATUS, 0, NULL, 0));
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(protocol.error_count == 2);

    assert(wqr_protocol_build_frame(frame, 0x10 | WQR_PAYLOAD_I2C, 0, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_build_frame(frame, 0x20 | WQR_PAYLOAD_STATUS, 1, NULL, 0));
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(protocol.error_count == 3);

    wqr_protocol_init(&protocol, &io);
    for (uint8_t sequence = 0; sequence < 8; ++sequence) {
        uint8_t fragments = sequence == 0 ? 0x10 : 0x20;

        assert(wqr_protocol_build_frame(frame, fragments | WQR_PAYLOAD_I2C, sequence, payload,
                                        sizeof(payload)));
        assert(wqr_protocol_receive(&protocol, frame));
    }
    assert(wqr_protocol_build_frame(frame, 0x40 | WQR_PAYLOAD_I2C, 8, last_payload,
                                    sizeof(last_payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(protocol.receive_length == WQR_TRANSFER_CAPACITY);
    wqr_protocol_poll(&protocol);
    assert(state.i2c_writes == 1);
    assert(state.i2c_length == WQR_TRANSFER_CAPACITY - 2);

    wqr_protocol_init(&protocol, NULL);
    for (uint8_t sequence = 0; sequence < 8; ++sequence) {
        uint8_t fragments = sequence == 0 ? 0x10 : 0x20;

        assert(wqr_protocol_build_frame(frame, fragments | WQR_PAYLOAD_I2C, sequence, payload,
                                        sizeof(payload)));
        assert(wqr_protocol_receive(&protocol, frame));
    }
    assert(protocol.receive_length == sizeof(payload) * 8);
    assert(wqr_protocol_build_frame(frame, 0x20 | WQR_PAYLOAD_I2C, 8, payload, sizeof(payload)));
    assert(!wqr_protocol_receive(&protocol, frame));
    assert(protocol.error_count == 1);
    assert(protocol.receive_length == 0);
    assert(!protocol.fragment_open);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 9, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, response));
    assert(response[1] == WQR_PAYLOAD_STATUS);
}

static void test_sequence_wrap(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .i2c_read = test_i2c_read};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t response[WQR_FRAME_SIZE];
    const uint8_t first[] = {0, 0xa1, 0x10};
    const uint8_t last[] = {1, 0};

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, 0x10 | WQR_PAYLOAD_I2C, 255, first, sizeof(first)));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(protocol.sequence == 0);
    assert(wqr_protocol_build_frame(frame, 0x40 | WQR_PAYLOAD_I2C, 0, last, sizeof(last)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, response));
    assert(response[2] == 1);
    assert(response[4] == 1);

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_STATUS, 255, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, response));
    assert(response[2] == 0);
    assert(wqr_protocol_build_frame(frame, 0, 255, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_response(&protocol, response));
    assert(response[1] == WQR_PAYLOAD_STATUS);
    assert(response[2] == 0);
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
    wqr_protocol_poll(&protocol);
    assert(!protocol.peer_ready_confirmed);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 0, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(state.transfer_control);
    assert(state.spi_transfers == 0);
    assert((frame[60] & 2) != 0);
    assert(protocol.peer_ready_confirmed);
    assert(!protocol.transfer_enabled);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 1, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(state.spi_transfers == 1);
    assert(frame[4] == 0x5a);
    assert(protocol.transfer_enabled);
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
    assert(!protocol.peer_ready_confirmed);
    assert(!protocol.transfer_enabled);

    payload[56] = 0;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 1, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(!state.transfer_control);
}

static void test_missing_spi_io(void) {
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};

    wqr_protocol_init(&protocol, NULL);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 0, payload, 1));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(protocol.peer_ready_confirmed);
    assert(!protocol.transfer_control_asserted);

    payload[56] = 1;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 1, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(protocol.transfer_enabled);

    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_ALTERNATE_SPI, 2, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(protocol.transfer_detail == 1);
}

static void test_pending_spi(void) {
    test_io state = {.transfer_ready = true};
    wqr_io io = {
        .context = &state,
        .spi_transfer = test_pending_spi_transfer,
        .transfer_ready = test_transfer_ready,
        .set_transfer_control = test_set_transfer_control,
    };
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};

    payload[0] = 0x5a;
    payload[56] = 1;
    wqr_protocol_init(&protocol, &io);
    protocol.peer_ready_confirmed = true;
    protocol.transfer_enabled = true;
    protocol.transfer_control_asserted = true;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_PRIMARY_SPI, 0, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(!wqr_protocol_response(&protocol, frame));
    assert(state.spi_transfers == 1);

    state.spi_complete = true;
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 0x5a);
    assert(state.spi_transfers == 1);
}

static void test_pending_i2c(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .i2c_read = test_pending_i2c_read};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t request[] = {0, 0xa1, 0x10, 3, 0};

    wqr_protocol_init(&protocol, &io);
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_I2C, 0, request, sizeof(request)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(!wqr_protocol_response(&protocol, frame));
    assert(state.i2c_started);
    assert(state.i2c_address == 0xa0);
    assert(state.i2c_command == 0x10);

    state.i2c_complete = true;
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[3] == 5);
    assert(frame[4] == 1);
    assert(frame[6] == 0x10);
    assert(frame[7] == 0x11);
    assert(frame[8] == 0x12);
}

static void test_pending_alternate_spi(void) {
    test_io state = {.transfer_ready = true};
    wqr_io io = {
        .context = &state,
        .spi_word = test_pending_spi_word,
        .transfer_ready = test_transfer_ready,
        .set_transfer_control = test_set_transfer_control,
    };
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0xcd, 0xab};

    payload[56] = 1;
    wqr_protocol_init(&protocol, &io);
    protocol.peer_ready_confirmed = true;
    protocol.transfer_enabled = true;
    protocol.transfer_control_asserted = true;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_ALTERNATE_SPI, 0, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(!wqr_protocol_response(&protocol, frame));
    assert(state.spi_word_transfers == 1);
    assert(state.spi_word == 0);

    state.spi_word_complete = true;
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[4] == 0x34);
    assert(frame[5] == 0x12);
    assert(state.spi_word_transfers == 1);

    state.spi_word_complete = false;
    assert(wqr_protocol_build_frame(frame, WQR_PAYLOAD_ALTERNATE_SPI, 1, payload, sizeof(payload)));
    assert(wqr_protocol_receive(&protocol, frame));
    wqr_protocol_poll(&protocol);
    assert(!wqr_protocol_response(&protocol, frame));
    assert(state.spi_word_transfers == 2);
    assert(state.spi_word == 0xabcd);

    state.transfer_ready = false;
    state.spi_word_complete = true;
    wqr_protocol_poll(&protocol);
    assert(wqr_protocol_response(&protocol, frame));
}

static void test_chunked_response(void) {
    test_io state = {0};
    wqr_io io = {.context = &state, .i2c_read = test_i2c_read};
    wqr_protocol protocol;
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t request[] = {0, 0xa1, 0x20, 120, 0};

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
    assert(frame[1] == (0x20 | WQR_PAYLOAD_I2C));
    assert(frame[2] == 13);
    assert(frame[3] == WQR_FRAME_PAYLOAD_SIZE);

    assert(wqr_protocol_build_frame(frame, 1, 13, NULL, 0));
    assert(wqr_protocol_receive(&protocol, frame));
    assert(wqr_protocol_response(&protocol, frame));
    assert(frame[1] == (0x40 | WQR_PAYLOAD_I2C));
    assert(frame[2] == 14);
    assert(frame[3] == 8);
}

static void test_sensor(void) {
    assert(wqr_sensor_value(0) == 999);
    assert(wqr_sensor_value(1) == 999);
    assert(wqr_sensor_value(2048) == 64);
    assert(wqr_sensor_value(4095) == -99);
    assert(wqr_sensor_value(4096) == -99);
}

int main(void) {
    test_crc();
    test_status_and_reset();
    test_status_values();
    test_fragmented_i2c_read();
    test_invalid_fragments();
    test_invalid_frame();
    test_control_frames();
    test_i2c_boundaries();
    test_rejected_input();
    test_sequence_wrap();
    test_primary_spi_handshake();
    test_transfer_not_ready();
    test_missing_spi_io();
    test_pending_spi();
    test_pending_i2c();
    test_pending_alternate_spi();
    test_chunked_response();
    test_sensor();
    return 0;
}
