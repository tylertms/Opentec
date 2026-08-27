#ifndef OPENTEC_BASE_SHIFTER_FRAME_H
#define OPENTEC_BASE_SHIFTER_FRAME_H

#include <stdint.h>

enum {
    SHIFTER_FRAME_SIZE = 13,
    SHIFTER_FRAME_PAYLOAD_SIZE = 4,
    SHIFTER_FRAME_START = 0x7b,
    SHIFTER_FRAME_END = 0x7d,
    SHIFTER_COMMAND_POSITION = 0x01,
    SHIFTER_COMMAND_REPLAY = 0x81,
};

typedef enum {
    SHIFTER_FRAME_VALID,
    SHIFTER_FRAME_INVALID_BOUNDARY,
    SHIFTER_FRAME_INVALID_CHECKSUM,
} ShifterFrameResult;

typedef struct {
    uint8_t command;
    uint8_t payload[SHIFTER_FRAME_PAYLOAD_SIZE];
    uint16_t primary_position;
    uint16_t secondary_position;
} ShifterFrame;

void shifter_frame_encode(const ShifterFrame *frame, uint8_t output[SHIFTER_FRAME_SIZE]);
ShifterFrameResult shifter_frame_decode(const uint8_t input[SHIFTER_FRAME_SIZE],
                                        ShifterFrame *frame);

#endif
