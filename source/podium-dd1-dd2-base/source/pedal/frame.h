#ifndef OPENTEC_BASE_PEDAL_FRAME_H
#define OPENTEC_BASE_PEDAL_FRAME_H

#include <stdint.h>

enum {
    PEDAL_FRAME_SIZE = 12,
    PEDAL_FRAME_PAYLOAD_SIZE = 8,
    PEDAL_FRAME_START = 0x7b,
    PEDAL_FRAME_END = 0x7d,
};

typedef enum {
    PEDAL_FRAME_VALID,
    PEDAL_FRAME_INVALID_BOUNDARY,
    PEDAL_FRAME_INVALID_CHECKSUM,
} PedalFrameResult;

typedef struct {
    uint8_t type;
    uint8_t payload[PEDAL_FRAME_PAYLOAD_SIZE];
} PedalFrame;

void pedal_frame_encode(const PedalFrame *frame, uint8_t output[PEDAL_FRAME_SIZE]);
PedalFrameResult pedal_frame_decode(const uint8_t input[PEDAL_FRAME_SIZE], PedalFrame *frame);

#endif
