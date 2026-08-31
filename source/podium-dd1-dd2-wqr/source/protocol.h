#ifndef OPENTEC_WQR_PROTOCOL_H
#define OPENTEC_WQR_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wqr_frame.h"

enum {
    WQR_TRANSFER_CAPACITY = 512,
    WQR_RESPONSE_CAPACITY = 514,
    WQR_SPI_TRANSFER_SIZE = 33,
    WQR_SPI_RESPONSE_SIZE = 57,
    WQR_STATUS_SIZE = 15
};

typedef enum { WQR_IO_PENDING, WQR_IO_SUCCEEDED, WQR_IO_FAILED } wqr_io_result;

typedef struct {
    void *context;
    wqr_io_result (*spi_transfer)(void *context, const uint8_t *transmit, uint8_t *receive,
                                  size_t length);
    wqr_io_result (*spi_word)(void *context, uint16_t transmit, uint16_t *receive);
    wqr_io_result (*i2c_write)(void *context, uint8_t address, const uint8_t *data, size_t length);
    wqr_io_result (*i2c_read)(void *context, uint8_t address, uint8_t command, uint8_t *data,
                              size_t length);
    uint8_t (*read_inputs)(void *context);
    bool (*transfer_ready)(void *context);
    bool (*transfer_control_ready)(void *context);
    void (*set_transfer_control)(void *context, bool asserted);
    void (*reset_transfer)(void *context);
    void (*request_reset)(void *context);
} wqr_io;

typedef struct {
    uint8_t receive_payload[WQR_TRANSFER_CAPACITY];
    uint8_t transmit_payload[WQR_RESPONSE_CAPACITY];
    uint8_t primary_response[WQR_SPI_RESPONSE_SIZE];
    uint8_t alternate_response[WQR_SPI_RESPONSE_SIZE];
    uint8_t last_fragment[WQR_FRAME_SIZE];
    uint8_t last_request[WQR_FRAME_SIZE];
    wqr_io io;

    size_t receive_length;
    size_t transmit_length;
    size_t transmit_offset;
    uint32_t error_count;
    uint32_t milliseconds;
    uint32_t seconds;

    int16_t sensor_value;
    uint8_t inputs;
    uint8_t payload_type;
    uint8_t response_type;
    uint8_t expected_command;
    uint8_t sequence;
    uint8_t transfer_detail;
    uint8_t command_marker;
    uint8_t transfer_state;
    uint8_t control_type;
    uint8_t control_payload;
    uint8_t control_sequence;

    bool peer_ready_confirmed;
    bool transfer_enabled;
    bool payload_pending;
    bool peripheral_transfer_active;
    bool response_ready;
    bool fragment_open;
    bool reset_after_response;
    bool transfer_control_asserted;
    bool alternate_spi_active;
    bool control_ready;
    bool last_fragment_valid;
    bool last_request_valid;
} wqr_protocol;

void wqr_protocol_init(wqr_protocol *protocol, const wqr_io *io);
bool wqr_protocol_receive(wqr_protocol *protocol, const uint8_t frame[WQR_FRAME_SIZE]);
bool wqr_protocol_response(const wqr_protocol *protocol, uint8_t frame[WQR_FRAME_SIZE]);
void wqr_protocol_response_sent(wqr_protocol *protocol);
void wqr_protocol_poll(wqr_protocol *protocol);
void wqr_protocol_tick(wqr_protocol *protocol);
void wqr_protocol_set_sensor_sample(wqr_protocol *protocol, uint16_t sample);
int16_t wqr_sensor_value(uint16_t sample);

#endif
