#ifndef OPENTEC_BASE_WHEEL_TRANSPORT_FRAME_H
#define OPENTEC_BASE_WHEEL_TRANSPORT_FRAME_H

#include <stdint.h>

enum {
    WHEEL_TRANSPORT_FRAME_SIZE = 64,
    WHEEL_TRANSPORT_PAYLOAD_SIZE = 57,
    WHEEL_TRANSPORT_FRAME_START = 0x7b,
    WHEEL_TRANSPORT_FRAME_END = 0x7d,
};

typedef enum {
    WHEEL_TRANSPORT_FRAME_VALID,
    WHEEL_TRANSPORT_FRAME_INVALID_BOUNDARY,
    WHEEL_TRANSPORT_FRAME_INVALID_LENGTH,
    WHEEL_TRANSPORT_FRAME_INVALID_CHECKSUM,
} WheelTransportFrameResult;

typedef struct {
    uint8_t command;
    uint8_t node;
    uint8_t length;
    uint8_t data[WHEEL_TRANSPORT_PAYLOAD_SIZE];
} WheelTransportFrame;

WheelTransportFrameResult wheel_transport_frame_encode(const WheelTransportFrame *frame,
                                                       uint8_t output[WHEEL_TRANSPORT_FRAME_SIZE]);
WheelTransportFrameResult
wheel_transport_frame_decode(const uint8_t input[WHEEL_TRANSPORT_FRAME_SIZE],
                             WheelTransportFrame *frame);

#endif
