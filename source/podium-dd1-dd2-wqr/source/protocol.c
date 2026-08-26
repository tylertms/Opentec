#include "protocol.h"

#include <string.h>

enum {
    FRAME_START = 0x7b,
    FRAME_END = 0x7d,
    FRAME_TYPE_MASK = 0x0f,
    FRAME_FIRST = 0x10,
    FRAME_MORE = 0x20,
    FRAME_LAST = 0x40,
    FRAME_FRAGMENT_MASK = 0x70,
    FRAME_TYPE_OFFSET = 1,
    FRAME_SEQUENCE_OFFSET = 2,
    FRAME_LENGTH_OFFSET = 3,
    FRAME_PAYLOAD_OFFSET = 4,
    FRAME_CRC_OFFSET = 61,
    FRAME_END_OFFSET = 63,
    I2C_RESPONSE_OVERHEAD = 2,
    I2C_FAILURE_RESPONSE_SIZE = 3,
    PRIMARY_RESPONSE_SIZE = 57,
    ALTERNATE_RESPONSE_SIZE = 57,
    SENSOR_TABLE_SIZE = 34
};

static const float sensor_resistance[SENSOR_TABLE_SIZE] = {
    3.368510e5f, 2.561158e5f, 1.964352e5f, 1.519174e5f, 1.184222e5f, 9.301190e4f, 7.358270e4f,
    5.861460e4f, 4.700000e4f, 3.792530e4f, 3.078810e4f, 2.513910e4f, 2.064080e4f, 1.703780e4f,
    1.413580e4f, 1.178580e4f, 9.872900e3f, 8.308100e3f, 7.021900e3f, 5.959700e3f, 5.078700e3f,
    4.344900e3f, 3.731000e3f, 3.215500e3f, 2.781000e3f, 2.413200e3f, 2.101000e3f, 1.834900e3f,
    1.607300e3f, 1.412200e3f, 1.244200e3f, 1.099300e3f, 9.738000e2f, 8.649000e2f};

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

uint16_t wqr_protocol_crc(const uint8_t *data, size_t length) {
    uint16_t crc = 0;

    while (length-- != 0) {
        unsigned int bit;

        crc ^= *data++;
        for (bit = 0; bit < 8; ++bit) {
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) != 0 ? 0x8408 : 0));
        }
    }

    return crc;
}

bool wqr_protocol_build_frame(uint8_t frame[WQR_FRAME_SIZE], uint8_t type_flags, uint8_t sequence,
                              const uint8_t *payload, size_t payload_length) {
    uint16_t crc;

    if (payload_length > WQR_FRAME_PAYLOAD_SIZE || (payload == NULL && payload_length != 0)) {
        return false;
    }

    memset(frame, 0, WQR_FRAME_SIZE);
    frame[0] = FRAME_START;
    frame[FRAME_TYPE_OFFSET] = type_flags;
    frame[FRAME_SEQUENCE_OFFSET] = sequence;
    frame[FRAME_LENGTH_OFFSET] = (uint8_t)payload_length;
    if (payload_length != 0) {
        memcpy(frame + FRAME_PAYLOAD_OFFSET, payload, payload_length);
    }
    crc = wqr_protocol_crc(frame + FRAME_TYPE_OFFSET, WQR_FRAME_BODY_SIZE);
    write_u16(frame + FRAME_CRC_OFFSET, crc);
    frame[FRAME_END_OFFSET] = FRAME_END;
    return true;
}

static bool frame_valid(const uint8_t frame[WQR_FRAME_SIZE]) {
    return frame[0] == FRAME_START && frame[FRAME_END_OFFSET] == FRAME_END &&
           frame[FRAME_LENGTH_OFFSET] <= WQR_FRAME_PAYLOAD_SIZE &&
           read_u16(frame + FRAME_CRC_OFFSET) ==
               wqr_protocol_crc(frame + FRAME_TYPE_OFFSET, WQR_FRAME_BODY_SIZE);
}

static bool transfer_ready(const wqr_protocol *protocol) {
    return protocol->io.transfer_ready == NULL || protocol->io.transfer_ready(protocol->io.context);
}

static void set_transfer_control(wqr_protocol *protocol, bool asserted) {
    if (protocol->io.set_transfer_control != NULL) {
        protocol->io.set_transfer_control(protocol->io.context, asserted);
    }
}

static void queue_payload(wqr_protocol *protocol, const uint8_t *payload, size_t length) {
    if (payload != protocol->transmit_payload) {
        memcpy(protocol->transmit_payload, payload, length);
    }
    protocol->transmit_length = length;
    protocol->transmit_offset = 0;
    protocol->response_type = protocol->payload_type;
    protocol->response_ready = true;
}

static void queue_control(wqr_protocol *protocol, bool acknowledged) {
    uint8_t payload = protocol->payload_type;

    queue_payload(protocol, &payload, 1);
    protocol->response_type = acknowledged ? 1 : 0;
}

static void encode_status(wqr_protocol *protocol, uint8_t status[WQR_STATUS_SIZE]) {
    memset(status, 0, WQR_STATUS_SIZE);
    status[0] = 7;
    if (protocol->io.read_inputs != NULL) {
        status[1] = protocol->io.read_inputs(protocol->io.context) & 7;
    }
    write_u16(status + 2, (uint16_t)protocol->sensor_value);
    write_u32(status + 4, protocol->seconds);
    write_u32(status + 8, protocol->error_count);
    status[12] = protocol->transfer_state;
    status[13] = protocol->transfer_detail;
    status[14] = protocol->command_marker;
}

static void process_primary_spi(wqr_protocol *protocol) {
    uint8_t response[PRIMARY_RESPONSE_SIZE] = {0};
    bool success = false;

    if (protocol->receive_length >= PRIMARY_RESPONSE_SIZE) {
        set_transfer_control(protocol, (protocol->receive_payload[56] & 1) != 0);
        if (protocol->io.spi_transfer != NULL && transfer_ready(protocol)) {
            success = protocol->io.spi_transfer(protocol->io.context, protocol->receive_payload,
                                                response, WQR_SPI_TRANSFER_SIZE);
        }
    }
    if (transfer_ready(protocol)) {
        response[56] |= 2;
    }
    protocol->transfer_state = success ? 2 : 0;
    queue_payload(protocol, response, sizeof(response));
}

static void process_alternate_spi(wqr_protocol *protocol) {
    uint8_t response[ALTERNATE_RESPONSE_SIZE] = {0};
    uint16_t received = 0;
    bool success = false;

    if (protocol->receive_length >= PRIMARY_RESPONSE_SIZE) {
        set_transfer_control(protocol, (protocol->receive_payload[56] & 1) != 0);
        if (protocol->io.spi_word != NULL && transfer_ready(protocol)) {
            success = protocol->io.spi_word(protocol->io.context,
                                            read_u16(protocol->receive_payload), &received);
        }
    }
    write_u16(response, received);
    if (transfer_ready(protocol)) {
        response[56] |= 2;
    }
    protocol->transfer_detail = 1;
    protocol->transfer_state = success ? 3 : 0;
    queue_payload(protocol, response, sizeof(response));
}

static void process_i2c(wqr_protocol *protocol) {
    uint8_t *request = protocol->receive_payload;
    uint8_t address;
    bool success = false;
    size_t response_length = I2C_FAILURE_RESPONSE_SIZE;

    if (protocol->receive_length < I2C_FAILURE_RESPONSE_SIZE) {
        memset(protocol->transmit_payload, 0, I2C_FAILURE_RESPONSE_SIZE);
        queue_payload(protocol, protocol->transmit_payload, I2C_FAILURE_RESPONSE_SIZE);
        return;
    }

    address = request[1] & 0xfe;
    memcpy(protocol->transmit_payload, request, I2C_FAILURE_RESPONSE_SIZE);
    if ((request[1] & 1) != 0) {
        size_t length;

        if (protocol->receive_length >= 5) {
            length = read_u16(request + 3);
            if (length <= WQR_TRANSFER_CAPACITY - I2C_RESPONSE_OVERHEAD &&
                protocol->io.i2c_read != NULL) {
                success = protocol->io.i2c_read(protocol->io.context, address, request[2],
                                                protocol->transmit_payload + I2C_RESPONSE_OVERHEAD,
                                                length);
                if (success) {
                    response_length = length + I2C_RESPONSE_OVERHEAD;
                }
            }
        }
    } else if (protocol->io.i2c_write != NULL) {
        success =
            protocol->io.i2c_write(protocol->io.context, address, request + I2C_RESPONSE_OVERHEAD,
                                   protocol->receive_length - I2C_RESPONSE_OVERHEAD);
    }
    protocol->transmit_payload[0] = success ? 1 : 0;
    queue_payload(protocol, protocol->transmit_payload, response_length);
}

static void process_status(wqr_protocol *protocol) {
    uint8_t status[WQR_STATUS_SIZE];

    protocol->command_marker =
        protocol->receive_length != 0 && protocol->receive_payload[0] == 0xaa ? 0xaa : 0;
    protocol->reset_after_response = protocol->command_marker == 0xaa;
    encode_status(protocol, status);
    queue_payload(protocol, status, sizeof(status));
}

static bool process_payload(wqr_protocol *protocol) {
    switch (protocol->payload_type) {
    case WQR_PAYLOAD_PRIMARY_SPI:
        process_primary_spi(protocol);
        return true;
    case WQR_PAYLOAD_ALTERNATE_SPI:
        process_alternate_spi(protocol);
        return true;
    case WQR_PAYLOAD_I2C:
        process_i2c(protocol);
        return true;
    case WQR_PAYLOAD_STATUS:
        process_status(protocol);
        return true;
    default:
        return false;
    }
}

static bool append_payload(wqr_protocol *protocol, const uint8_t *payload, size_t length) {
    if (length > WQR_TRANSFER_CAPACITY - protocol->receive_length) {
        return false;
    }
    memcpy(protocol->receive_payload + protocol->receive_length, payload, length);
    protocol->receive_length += length;
    return true;
}

void wqr_protocol_init(wqr_protocol *protocol, const wqr_io *io) {
    memset(protocol, 0, sizeof(*protocol));
    protocol->payload_type = 0xff;
    if (io != NULL) {
        protocol->io = *io;
    }
}

bool wqr_protocol_receive(wqr_protocol *protocol, const uint8_t frame[WQR_FRAME_SIZE]) {
    uint8_t type_flags;
    uint8_t type;
    uint8_t fragments;

    ++protocol->frame_count;
    if (!frame_valid(frame)) {
        ++protocol->error_count;
        queue_control(protocol, false);
        return false;
    }

    type_flags = frame[FRAME_TYPE_OFFSET];
    type = type_flags & FRAME_TYPE_MASK;
    fragments = type_flags & FRAME_FRAGMENT_MASK;

    if (type == 0) {
        bool sequence_is_earlier = frame[FRAME_SEQUENCE_OFFSET] < protocol->sequence ||
                                   (protocol->sequence == 0 && frame[FRAME_SEQUENCE_OFFSET] != 0);

        if (protocol->response_ready && sequence_is_earlier) {
            return true;
        }
        queue_control(protocol, false);
        return false;
    }
    if (type == 1) {
        if (!protocol->response_ready ||
            protocol->transmit_offset + WQR_FRAME_PAYLOAD_SIZE >= protocol->transmit_length) {
            protocol->response_ready = false;
            return true;
        }
        protocol->transmit_offset += WQR_FRAME_PAYLOAD_SIZE;
        ++protocol->sequence;
        return true;
    }
    if (type < WQR_PAYLOAD_PRIMARY_SPI || type > WQR_PAYLOAD_STATUS) {
        queue_control(protocol, false);
        return false;
    }

    if (fragments == 0 || fragments == FRAME_FIRST) {
        protocol->receive_length = 0;
        protocol->payload_type = type;
    } else if (protocol->payload_type != type) {
        ++protocol->error_count;
        queue_control(protocol, false);
        return false;
    }
    if (!append_payload(protocol, frame + FRAME_PAYLOAD_OFFSET, frame[FRAME_LENGTH_OFFSET])) {
        ++protocol->error_count;
        protocol->receive_length = 0;
        queue_control(protocol, false);
        return false;
    }

    protocol->sequence = (uint8_t)(frame[FRAME_SEQUENCE_OFFSET] + 1);
    if (fragments == FRAME_FIRST || fragments == FRAME_MORE) {
        queue_control(protocol, true);
        protocol->payload_type = type;
        return true;
    }
    if (fragments != 0 && fragments != FRAME_LAST) {
        ++protocol->error_count;
        queue_control(protocol, false);
        return false;
    }
    if (!process_payload(protocol)) {
        queue_control(protocol, false);
        return false;
    }
    protocol->receive_length = 0;
    return true;
}

bool wqr_protocol_response(const wqr_protocol *protocol, uint8_t frame[WQR_FRAME_SIZE]) {
    size_t remaining;
    size_t length;
    uint8_t type_flags;

    if (!protocol->response_ready || protocol->transmit_offset > protocol->transmit_length) {
        return false;
    }

    remaining = protocol->transmit_length - protocol->transmit_offset;
    length = remaining < WQR_FRAME_PAYLOAD_SIZE ? remaining : WQR_FRAME_PAYLOAD_SIZE;
    type_flags = protocol->response_type & FRAME_TYPE_MASK;
    if (protocol->transmit_length > WQR_FRAME_PAYLOAD_SIZE) {
        type_flags |= protocol->transmit_offset == 0       ? FRAME_FIRST
                      : remaining > WQR_FRAME_PAYLOAD_SIZE ? FRAME_MORE
                                                           : FRAME_LAST;
    }
    return wqr_protocol_build_frame(frame, type_flags, protocol->sequence,
                                    protocol->transmit_payload + protocol->transmit_offset, length);
}

void wqr_protocol_response_sent(wqr_protocol *protocol) {
    if (protocol->reset_after_response && protocol->io.request_reset != NULL) {
        protocol->reset_after_response = false;
        protocol->io.request_reset(protocol->io.context);
    }
}

void wqr_protocol_tick(wqr_protocol *protocol) {
    ++protocol->milliseconds;
    if (protocol->milliseconds == 1000) {
        protocol->milliseconds = 0;
        ++protocol->seconds;
    }
}

int16_t wqr_sensor_value(uint16_t sample) {
    float voltage;
    float resistance;
    size_t index = 0;

    if (sample == 0) {
        return 999;
    }
    voltage = (float)sample * 3.3f / 4096.0f;
    resistance = 10000.0f / (3.3f / voltage + 1.0f);
    while (index < SENSOR_TABLE_SIZE && resistance < sensor_resistance[index]) {
        ++index;
    }
    if (index == 0) {
        return -99;
    }
    if (index == SENSOR_TABLE_SIZE) {
        return 999;
    }
    return (int16_t)((((sensor_resistance[index - 1] - resistance) /
                       (sensor_resistance[index - 1] - sensor_resistance[index])) +
                      (float)(index - 1)) *
                         5.0f +
                     15.0f);
}

void wqr_protocol_set_sensor_sample(wqr_protocol *protocol, uint16_t sample) {
    protocol->raw_sensor_sample = sample;
    protocol->sensor_value = wqr_sensor_value(sample);
}
