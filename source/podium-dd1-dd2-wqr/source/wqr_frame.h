#ifndef OPENTEC_COMMON_WQR_FRAME_H
#define OPENTEC_COMMON_WQR_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WQR_FRAME_SIZE = 64,
    WQR_FRAME_BODY_SIZE = 60,
    WQR_FRAME_PAYLOAD_SIZE = 57,
    WQR_FRAME_TYPE_MASK = 0x0f,
    WQR_FRAME_FIRST = 0x10,
    WQR_FRAME_MORE = 0x20,
    WQR_FRAME_LAST = 0x40,
    WQR_FRAME_FRAGMENT_MASK = 0x70,
    WQR_FRAME_NACK = 0,
    WQR_FRAME_ACK = 1,
    WQR_PAYLOAD_PRIMARY_SPI = 2,
    WQR_PAYLOAD_ALTERNATE_SPI = 3,
    WQR_PAYLOAD_I2C = 4,
    WQR_PAYLOAD_STATUS = 5
};

typedef struct {
    const uint8_t *payload;
    size_t payload_length;
    uint8_t type_flags;
    uint8_t sequence;
} wqr_frame_view;

uint16_t wqr_frame_crc(const uint8_t *data, size_t length);
bool wqr_frame_build(uint8_t frame[WQR_FRAME_SIZE], uint8_t type_flags, uint8_t sequence,
                     const uint8_t *payload, size_t payload_length);
bool wqr_frame_parse(const uint8_t frame[WQR_FRAME_SIZE], wqr_frame_view *view);

#endif
